#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <execinfo.h>
#include <pwd.h>
#include <grp.h>

#include <iostream>
#include <algorithm>
#include <boost/asio.hpp>
#include <boost/system/linux_error.hpp>
#include <boost/exception/all.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/sources/channel_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/init/to_console.hpp>
#include <boost/log/utility/init/common_attributes.hpp>
#include <boost/log/filters.hpp>
#include <boost/filesystem.hpp>

#include <pthread.h>

#include "fastproxy.hpp"
#include "proxy.hpp"
#include "statistics.hpp"

fastproxy* fastproxy::instance_;
logger fastproxy::log = logger(keywords::channel = "fastproxy");

fastproxy::fastproxy()
{
    instance_ = this;
}

fastproxy& fastproxy::instance()
{
    return *instance_;
}

typedef std::vector<std::string> string_vec;

void fastproxy::parse_config(int argc, char* argv[])
{
    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "produce help message")
            ("ingoing-http", po::value<ip::tcp::endpoint>()->required(), "http listening address")
            ("ingoing-stat", po::value<std::string>()->required(), "statistics listening socket")
            ("outgoing-http", po::value<ip::tcp::endpoint>()->default_value(ip::tcp::endpoint()), "outgoing address for HTTP requests")
            ("outgoing-ns", po::value<ip::udp::endpoint>()->default_value(ip::udp::endpoint()), "outgoing address for NS lookup")
            ("name-server", po::value<ip::udp::endpoint>()->required(), "name server address")
            ("log-level", po::value<int>()->default_value(2), "logging level")
            ("log-channel", po::value<string_vec>(), "logging channel")
            ("receive-timeout", po::value<time_duration::sec_type>()->default_value(3600), "timeout for receive operations (in seconds)")
            ("allow-header", po::value<string_vec>()->default_value(string_vec(), "any"), "allowed header for requests")
            ("stat-socket-user", po::value<std::string>()->default_value(getpwuid(getuid())->pw_name), "user for statistics socket")
            ("stat-socket-group", po::value<std::string>()->default_value(getgrgid(getgid())->gr_name), "group for statistics socket")
            ("stop-after-init", po::value<bool>()->default_value(false), "raise SIGSTOP after initialization (Upstart support)");

    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            exit(1);
        }

        po::notify(vm);
    }
    catch (const boost::program_options::required_option& exc)
    {
        std::cout << desc << std::endl;
        throw;
    }
}

bool check_channel(const std::string& channel)
{
    return fastproxy::instance().check_channel_impl(channel);
}

bool fastproxy::check_channel_impl(const std::string& channel) const
{
    return channels.find(channel) != channels.end();
}

void fastproxy::init_logging()
{
    if (vm.count("log-channel"))
    {
        auto chans = vm["log-channel"].as<string_vec>();
        channels.insert(chans.begin(), chans.end());
    }
    boost::log::add_common_attributes();
    boost::log::init_log_to_console
    (
            std::cerr,
            keywords::format = "[%TimeStamp%]: %Channel%: %_%"
    );
    boost::log::core::get()->set_filter
    (
        boost::log::filters::attr<severity_level>("Severity") >= vm["log-level"].as<int>() &&
        boost::log::filters::attr<std::string>("Channel").satisfies(&check_channel)
    );
}

void fastproxy::init_signals()
{
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    int s = pthread_sigmask(SIG_BLOCK, &set, 0);
    if (s != 0)
    {
        perror("pthread_sigmask");
        exit(EXIT_FAILURE);
    }

    sw.reset(new signal_waiter(io));
    sw->add_signal(SIGTERM);
    sw->add_signal(SIGQUIT);
    sw->add_signal(SIGINT);
    start_waiting_for_quit();
}

void fastproxy::start_waiting_for_quit()
{
    sw->async_wait(boost::bind(&fastproxy::quit, this, placeholders::error()));
}

void fastproxy::quit(const error_code& ec)
{
    if (ec)
    {
        TRACE_ERROR(ec);
        start_waiting_for_quit();
    }
    else
    {
        io.stop();
    }
}

void fastproxy::init_statistics()
{
    const std::string& stat_sock = vm["ingoing-stat"].as<std::string>();
    boost::filesystem::remove(stat_sock);
    s.reset(new statistics(io, stat_sock));
    int res = chown(stat_sock.c_str(),
        getpwnam(vm["stat-socket-user"].as<std::string>().c_str())->pw_uid,
        getgrnam(vm["stat-socket-group"].as<std::string>().c_str())->gr_gid);
    if (res != 0)
        perror(("chown(" + stat_sock + ", " + vm["stat-socket-user"].as<std::string>() + ", " + vm["stat-socket-group"].as<std::string>() + ")").c_str());
}

void fastproxy::init_proxy()
{
    p.reset(new proxy(io, vm["ingoing-http"].as<ip::tcp::endpoint>(),
            vm["outgoing-http"].as<ip::tcp::endpoint>(),
            vm["outgoing-ns"].as<ip::udp::endpoint>(),
            vm["name-server"].as<ip::udp::endpoint>(),
            boost::posix_time::seconds(vm["receive-timeout"].as<time_duration::sec_type>()),
            vm["allow-header"].as<string_vec>()));
}

void fastproxy::init_resolver()
{
    resolver::init();
}

template<class stream_type, class protocol>
bool operator >> (stream_type& stream, ip::basic_endpoint<protocol>& endpoint)
{
    std::string str;
    stream >> str;
    std::string::iterator colon = std::find(str.begin(), str.end(), ':');
    endpoint.address(ip::address::from_string(std::string(str.begin(), colon)));
    if (colon != str.end())
        endpoint.port(atoi(&*colon + 1));
    return true;
}

proxy* fastproxy::find_proxy()
{
    return p.get();
}

void fastproxy::init(int argc, char* argv[])
{
    parse_config(argc, argv);
    init_logging();
    init_resolver();
    init_signals();

    init_statistics();
    init_proxy();

    if (vm["stop-after-init"].as<bool>())
        raise(SIGSTOP);
}

void fastproxy::run()
{
    s->start();
    p->start();

    io.run();
}

void terminate()
{
    void* array[50];
    int size = backtrace(array, 50);

    std::cerr << "backtrace returned " << size << " frames" << std::endl;

    char ** messages = backtrace_symbols(array, size);

    for (int i = 0; i < size && messages != NULL; ++i)
    {
        std::cerr << "[bt]: (" << i << ") " << messages[i] << std::endl;
    }
    std::cerr << std::endl;

    free(messages);

    abort();
}


int main(int argc, char* argv[])
{
//    try
//    {
//        std::set_terminate(terminate);

        fastproxy f;
        f.init(argc, argv);
        f.run();
//    }
//    catch (const boost::exception& e)
//    {
//        std::cerr << boost::diagnostic_information(e);
//    }
    return 0;
}
