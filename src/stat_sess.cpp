/*
 * stat_sess.cpp
 *
 *  Created on: Jul 13, 2010
 *      Author: nbryskin
 */

#include <boost/bind.hpp>

#include "stat_sess.hpp"
#include "statistics.hpp"

logger statistics_session::log = logger(keywords::channel = "statistics_session");

statistics_session::statistics_session(asio::io_service& io, statistics& parent)
  : sock(io)
  , parent(parent)
{
}

local::stream_protocol::socket& statistics_session::socket()
{
    return sock;
}

void statistics_session::start()
{
    start_waiting_request();
}

void statistics_session::start_waiting_request()
{
    asio::async_read_until(sock, buffer, '\n', boost::bind(&statistics_session::finished_waiting_request, this, placeholders::error()));
}

void statistics_session::finished_waiting_request(const error_code& ec)
{
    if (ec)
    {
        TRACE_ERROR(ec);
        return finished(ec);
    }

    std::string request;
    {
        std::istream is(&buffer);
        std::getline(is, request);
    }

    {
        std::ostream os(&buffer);
        os << parent.process_request(request);
    }

    start_sending_response();
}

void statistics_session::start_sending_response()
{
    asio::async_write(sock, buffer, boost::bind(&statistics_session::finished_sending_response, this, placeholders::error()));
}

void statistics_session::finished_sending_response(const error_code& ec)
{
    if (ec)
    {
        TRACE_ERROR(ec);
        return finished(ec);
    }

    start_waiting_request();
}

void statistics_session::finished(const error_code& ec)
{
    parent.finished_session(this, ec);
}
