#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

#include <iostream>
#include <algorithm>
#include <boost/asio.hpp>
#include <boost/system/linux_error.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/sources/channel_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/init/to_console.hpp>
#include <boost/log/utility/init/common_attributes.hpp>
#include <boost/log/filters.hpp>

#include <pthread.h>

#include "fastproxy.hpp"
#include "proxy.hpp"
#include "statistics.hpp"
#include "chater.hpp"

fastproxy* fastproxy::instance_;

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
            ("inbound", po::value<ip::tcp::endpoint>()->required(), "listening address")
            ("outbound-http", po::value<ip::tcp::endpoint>()->required(), "outgoing address for HTTP requests")
            ("outbound-ns", po::value<ip::udp::endpoint>()->required(), "outgoing address for NS lookup")
            ("name-server", po::value<ip::udp::endpoint>()->required(), "name server address")
            ("stat-interval", po::value<int>()->required(), "interval of statistics dumping")
            ("log-level", po::value<int>()->required(), "logging level")
            ("log-channel", po::value<string_vec>(), "logging channel");

    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        exit(1);
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
}

void fastproxy::init_statistics()
{
    s.reset(new statistics(io, vm["stat-interval"].as<int>()));
}

void fastproxy::init_proxy()
{
    p.reset(new proxy(io, vm["inbound"].as<ip::tcp::endpoint>(),
            vm["outbound-http"].as<ip::tcp::endpoint>(),
            vm["outbound-ns"].as<ip::udp::endpoint>(),
            vm["name-server"].as<ip::udp::endpoint>()));
}

void fastproxy::init_chater()
{
    c.reset(new chater(io, 1));
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
    init_chater();
}

void fastproxy::run()
{
    s->start();
    p->start();
    c->start();

    io.run();
}

int main(int argc, char* argv[])
{
    fastproxy f;
    f.init(argc, argv);
    f.run();
    return 0;
}
