/*
 * fastproxy.hpp
 *
 *  Created on: Jun 2, 2010
 *      Author: nbryskin
 */

#ifndef FASTPROXY_HPP_
#define FASTPROXY_HPP_

#include <set>
#include <boost/program_options.hpp>

#include "common.hpp"

class proxy;
class statistics;
class chater;

namespace po = boost::program_options;

class fastproxy
{
public:
    static fastproxy& instance();

    fastproxy();

    void init(int argc, char* argv[]);
    void run();

    proxy* find_proxy();

private:
    void parse_config(int argc, char* argv[]);
    void init_logging();
    void init_resolver();
    void init_signals();
    void init_statistics();
    void init_proxy();
    void init_chater();

    bool check_channel_impl(const std::string& channel) const;
    friend bool check_channel(const std::string& channel);

    po::variables_map vm;
    asio::io_service io;
    std::unique_ptr<statistics> s;
    std::unique_ptr<proxy> p;
    std::unique_ptr<chater> c;
    std::set<std::string> channels;
    static fastproxy* instance_;
    static logger log;
};

#endif /* FASTPROXY_HPP_ */
