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
#include <boost/program_options.hpp>
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
#include "common.hpp"

namespace po = boost::program_options;

po::variables_map parse_config(int argc, char* argv[])
{
    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "produce help message")
            ("inbound", po::value<ip::tcp::endpoint>()->required(), "listening address")
            ("outbound-http", po::value<ip::tcp::endpoint>()->required(), "outgoing address for HTTP requests")
            ("outbound-ns", po::value<ip::udp::endpoint>()->required(), "outgoing address for NS lookup")
            ("name-server", po::value<ip::udp::endpoint>()->required(), "name server address")
            ("stat-output", po::value<std::string>()->required(), "output for statistics")
            ("stat-interval", po::value<int>()->required(), "interval of statistics dumping")
            ("log-level", po::value<int>()->required(), "logging level");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        exit(1);
    }

    return vm;
}

void init_logging(const po::variables_map& vm)
{
    boost::log::add_common_attributes();
    boost::log::init_log_to_console
    (
            std::clog,
            keywords::format = "[%TimeStamp%]: %Channel%: %_%"
    );
    boost::log::core::get()->set_filter
    (
        boost::log::filters::attr<severity_level>("Severity") >= vm["log-level"].as<int>()
    );
}

void init_signals()
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

statistics* init_statistics(asio::io_service& io, const po::variables_map& vm)
{
    return new statistics(io, vm["stat-output"].as<std::string>(), vm["stat-interval"].as<int>());
}

proxy* init_proxy(asio::io_service& io, const po::variables_map& vm)
{
    return new proxy(io, vm["inbound"].as<ip::tcp::endpoint>(),
            vm["outbound-http"].as<ip::tcp::endpoint>(),
            vm["outbound-ns"].as<ip::udp::endpoint>(),
            vm["name-server"].as<ip::udp::endpoint>());
}

chater* init_chater(asio::io_service& io)
{
    return new chater(io, 1);
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

proxy* global_proxy;

proxy* find_proxy()
{
    return global_proxy;
}

int main(int argc, char* argv[])
{
    po::variables_map vm = parse_config(argc, argv);
    init_logging(vm);
    init_resolver();
    init_signals();

    asio::io_service io;
    std::unique_ptr<statistics> s(init_statistics(io, vm));
    std::unique_ptr<proxy> p(init_proxy(io, vm));
    std::unique_ptr<chater> c(init_chater(io));

    global_proxy = p.get();

    s->start();
    p->start();
    c->start();

    io.run();

    return 0;
}
