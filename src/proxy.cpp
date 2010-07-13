/*
 * proxy.cpp
 *
 *  Created on: May 31, 2010
 *      Author: nbryskin
 */

#include <iostream>
#include <functional>
#include <boost/bind.hpp>

#include "proxy.hpp"
#include "statistics.hpp"
#include "session.hpp"

logger proxy::log = logger(keywords::channel = "proxy");

bool session_less(const session& lhs, const session& rhs)
{
    return lhs.get_id() < rhs.get_id();
}

proxy::proxy(asio::io_service& io, const ip::tcp::endpoint& inbound, const ip::tcp::endpoint& outbound_http,
             const ip::udp::endpoint& outbound_ns, const ip::udp::endpoint& name_server,
             const time_duration& receive_timeout, const std::vector<std::string>& allowed_headers)
    : acceptor(io, inbound)
    , resolver_(io, outbound_ns, name_server)
    , outbound_http(outbound_http)
    , receive_timeout(receive_timeout)
    , sessions(std::ptr_fun(session_less))
    , headers_cont(allowed_headers)
{
    for (auto it = headers_cont.begin(); it != headers_cont.end(); ++it)
        this->allowed_headers.insert(it->c_str());
    TRACE() << "start listening on " << inbound;
    acceptor.listen();
}

// called by main (parent)
void proxy::start()
{
    start_accept();
    resolver_.start();
    TRACE() << "started";
}

// called by session (child)
resolver& proxy::get_resolver()
{
    return resolver_;
}

// called by session (child)
void proxy::finished_session(session* session, const boost::system::error_code& ec)
{
    TRACE_ERROR(ec) << session->get_id();
    std::size_t c = sessions.erase(*session);
    if (c != 1)
        TRACE() << "erased " << c << " items. total " << sessions.size() << " items";
    assert(c == 1);
    update_statistics();
}

void proxy::start_accept()
{
    std::unique_ptr<session> new_sess(new session(acceptor.io_service(), *this));
    acceptor.async_accept(new_sess->socket(), boost::bind(&proxy::handle_accept, this, placeholders::error(), new_sess.get()));
    new_sess.release();
}

void proxy::handle_accept(const boost::system::error_code& ec, session* new_session)
{
    std::unique_ptr<session> session_ptr(new_session);
    TRACE_ERROR(ec);
    if (ec)
        return;

    start_session(session_ptr.get());
    session_ptr.release();
}

void proxy::start_session(session* new_session)
{
    TRACE() << new_session;
    bool inserted = sessions.insert(new_session).second;
    assert(inserted);
    new_session->start();
    start_accept();
    update_statistics();
}

void proxy::update_statistics()
{
    statistics::push("sesscnt", sessions.size());
}

void proxy::dump_channels_state() const
{
    for (session_cont::const_iterator it = sessions.begin(); it != sessions.end(); ++it)
    {
        BOOST_LOG_SEV(log, severity_level::debug)
                << it->get_id()
                << " reqch: " << it->get_request_channel().get_state()
                << " rspch: " << it->get_response_channel().get_state()
                << " opened: " << it->get_opened_channels();
    }
}

const ip::tcp::endpoint& proxy::get_outgoing_endpoint() const
{
    return outbound_http;
}

const time_duration& proxy::get_receive_timeout() const
{
    return receive_timeout;
}

const headers_type& proxy::get_allowed_headers() const
{
    return allowed_headers;
}
