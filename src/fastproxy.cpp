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

#include "proxy.hpp"
#include "common.hpp"

namespace po = boost::program_options;

po::variables_map parse_config(int argc, char* argv[])
{
    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "produce help message")
            ("listen", po::value<boost::uint16_t>()->required(), "listening port")
            ("outgoing", po::value<std::string>()->required(), "outgoing address")
            ("name-server", po::value<std::string>()->required(), "name server address");

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

void init_logging()
{
    boost::log::add_common_attributes();
    boost::log::init_log_to_console
    (
            std::clog,
            keywords::format = "[%TimeStamp%]: %Channel%: %_%"
    );
    boost::log::core::get()->set_filter
    (
        boost::log::filters::attr<severity_level>("Severity") >= severity_level::debug
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

int main(int argc, char* argv[])
{
    po::variables_map vm = parse_config(argc, argv);
    init_logging();
    init_resolver();
    init_signals();

    ip::tcp::endpoint inbound(ip::tcp::v4(), vm["listen"].as<boost::uint16_t> ());
    ip::udp::endpoint outbound(ip::address::from_string(vm["outgoing"].as<std::string> ()), 0);
    ip::udp::endpoint name_server(ip::address::from_string(vm["name-server"].as<std::string> ()), 53);

    asio::io_service io;
    proxy p(io, inbound, outbound, name_server);
    p.start();

    io.run();

    return 0;
}
