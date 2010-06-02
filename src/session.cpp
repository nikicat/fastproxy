/*
 * session.cpp
 *
 *  Created on: May 24, 2010
 *      Author: nbryskin
 */

#include <functional>
#include <algorithm>
#include <boost/bind.hpp>
#include <boost/lambda/lambda.hpp>

#include "session.hpp"
#include "proxy.hpp"
#include "statistics.hpp"

logger session::log = logger(keywords::channel = "session");

session::session(asio::io_service& io, proxy& parent_proxy)
    : parent_proxy(parent_proxy), requester(io), responder(io), resolver(io)
    , request_channel(requester, responder, this)
    , response_channel(responder, requester, this)
    , opened_channels(0)
    , resolve_handler(boost::bind(&session::finished_resolving, this, placeholders::error(), _2, _3))
{
}

ip::tcp::socket& session::socket()
{
    return requester;
}

void session::start()
{
    start_receive_header();
}

void session::finish(const error_code& ec)
{
    opened_channels--;
    TRACE_ERROR(ec);
    if (ec)
    {
        BOOST_LOG_SEV(log, severity_level::error) << system_error(ec, "channel error").what();
        error_code tmp_ec;
        requester.cancel(tmp_ec);
        responder.cancel(tmp_ec);
    }
    if (opened_channels == 0)
    {
        statistics::push("sesstm", timer.elapsed());
        parent_proxy.finished_session(this, ec ? ec : prev_ec);
    }
    else
    {
        prev_ec = ec;
        statistics::push("chantm", timer.elapsed());
    }
}

void session::start_receive_header()
{
    timer.restart();
    requester.async_receive(asio::buffer(header), requester.message_peek, boost::bind(&session::finished_receive_header, this,
            placeholders::error(), placeholders::bytes_transferred));
}

void session::finished_receive_header(const error_code& ec, std::size_t bytes_transferred)
{
    statistics::push("headtm", timer.elapsed());
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);
    start_resolving(parse_header(bytes_transferred));
}

void session::start_resolving(const char* peer)
{
    TRACE() << peer << ":" << port;
    timer.restart();
    parent_proxy.get_resolver().async_resolve(peer, resolve_handler);
}

void session::finished_resolving(const error_code& ec, resolver::const_iterator begin, resolver::const_iterator end)
{
    statistics::push("restm", timer.elapsed());
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);
    // TODO: cycle throw all addresses
    start_connecting_to_peer(ip::tcp::endpoint(*begin, port));
}

void session::start_connecting_to_peer(const ip::tcp::endpoint& peer)
{
    TRACE() << peer;
    timer.restart();
    responder.async_connect(peer, boost::bind(&session::finished_connecting_to_peer, this, placeholders::error()));
}

void session::finished_connecting_to_peer(const error_code& ec)
{
    statistics::push("contm", timer.elapsed());
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);
    timer.restart();
    opened_channels++;
    request_channel.start();
    opened_channels++;
    response_channel.start();
}

const char* session::parse_header(std::size_t size)
{
    using boost::lambda::_1;
    char* begin = std::find(header.begin(), header.begin() + size, ' ') + 8;
    char* end = std::find_if(begin, header.begin() + size, _1 == ' ' || _1 == '/');
    char* colon = std::find(begin, end, ':');
    *end = 0;
    *colon = 0;
    // TODO: replace with c++-style conversion
    port = (colon == end ? default_http_port : std::uint16_t(atoi(colon + 1)));
    return begin;
}

const channel& session::get_request_channel() const
{
    return request_channel;
}

const channel& session::get_response_channel() const
{
    return response_channel;
}

bool session::operator < (const session& other) const
{
    return const_cast<ip::tcp::socket&>(requester).native() < const_cast<ip::tcp::socket&>(other.requester).native();
}
