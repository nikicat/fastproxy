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

channel_logger session::log = channel_logger(logging::keywords::channel = "session");

session::session(io_service& io, proxy& parent_proxy)
    : parent_proxy(parent_proxy), requester(io), responder(io), resolver(io)
    , request_channel(requester, responder, this)
    , response_channel(responder, requester, this)
    , opened_channels(0)
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
    if (opened_channels == 0)
        parent_proxy.finished_session(this, ec);
}

void session::start_receive_header()
{
    requester.async_receive(buffer(header), requester.message_peek, boost::bind(&session::finished_receive_header, this,
            placeholders::error(), placeholders::bytes_transferred));
}

void session::finished_receive_header(const error_code& ec, std::size_t bytes_transferred)
{
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);
    std::pair<std::string, std::uint16_t> host_port = parse_header(bytes_transferred);
    start_resolving(host_port);
}

void session::start_resolving(const std::pair<std::string, std::uint16_t>& peer)
{
    TRACE() << peer.first << ":" << peer.second;
    parent_proxy.get_resolver().async_resolve(peer.first, boost::bind(&session::finished_resolving, this, placeholders::error(), _2, _3, peer.second));
}

void session::finished_resolving(const error_code& ec, resolver::const_iterator begin, resolver::const_iterator end, std::uint16_t port)
{
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);
    // TODO: cycle throw all addresses
    start_connecting_to_peer(ip::tcp::endpoint(*begin, port));
}

void session::start_connecting_to_peer(const ip::tcp::endpoint& peer)
{
    TRACE() << peer;
    responder.async_connect(peer, boost::bind(&session::finished_connecting_to_peer, this, placeholders::error()));
}

void session::finished_connecting_to_peer(const error_code& ec)
{
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);
    opened_channels++;
    request_channel.start();
    opened_channels++;
    response_channel.start();
}

std::pair<std::string, std::uint16_t> session::parse_header(std::size_t size)
{
    using boost::lambda::_1;
    char* begin = std::find(header.begin(), header.begin() + size, ' ') + 8;
    char* end = std::find_if(begin, header.begin() + size, _1 == ' ' || _1 == '/');
    char* colon = std::find(begin, end, ':');
    *end = 0;
    // TODO: replace with c++-style conversion
    return std::make_pair(std::string(begin, colon), colon == end ? default_http_port : std::uint16_t(atoi(colon + 1)));
}
