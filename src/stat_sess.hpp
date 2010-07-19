/*
 * stat_sess.hpp
 *
 *  Created on: Jul 13, 2010
 *      Author: nbryskin
 */

#ifndef STAT_SESS_HPP_
#define STAT_SESS_HPP_

#include <boost/asio.hpp>

#include "common.hpp"

class statistics;

class statistics_session : public boost::noncopyable
{
public:
    statistics_session(asio::io_service& io, statistics& parent);

    local::stream_protocol::socket& socket();

    void start();

private:
    void start_waiting_request();
    void finished_waiting_request(const error_code& ec);
    void start_sending_response();
    void finished_sending_response(const error_code& ec);

    void finished(const error_code& ec);

    asio::local::stream_protocol::socket sock;
    asio::streambuf buffer;
    statistics& parent;
    static logger log;
};


#endif /* STAT_SESS_HPP_ */
