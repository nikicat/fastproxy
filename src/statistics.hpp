/*
 * statistics.hpp
 *
 *  Created on: May 31, 2010
 *      Author: nbryskin
 */

#ifndef STATISTICS_HPP_
#define STATISTICS_HPP_

#include <deque>
#include <algorithm>
#include <fstream>
#include <numeric>
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/ptr_container/ptr_set.hpp>
#include <boost/asio.hpp>
#include <boost/variant.hpp>

#include "stat_sess.hpp"
#include "common.hpp"

class statistics
{
public:
    statistics(asio::io_service& io, const local::stream_protocol::endpoint& stat_ep);

    void start();

    static statistics& instance();

    static void increment(const char* name, long value = 1);
    static void decrement(const char* name, long value = 1);
    static void increment(const char* name, double value);

    std::string process_request(const std::string& request) const;

    // called by statistics_session (child)
    void finished_session(statistics_session* session, const boost::system::error_code& ec);

protected:
    void start_accept();
    void finished_accept(const error_code& ec, statistics_session* new_session);
    void start_session(statistics_session* new_session);

private:
    typedef boost::variant<long, double> value_t;
    value_t get_statistic(const std::string& name) const;

    template<typename T> void add(const char* name, T value);

    typedef std::map<const char*, value_t> counters_t;
    counters_t counters;
    local::stream_protocol::acceptor acceptor;

    typedef boost::ptr_set<statistics_session> sessions_t;
    sessions_t sessions;

    static statistics* instance_;
    static logger log;
};

#endif /* STATISTICS_HPP_ */
