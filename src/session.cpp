/*
 * session.cpp
 *
 *  Created on: May 24, 2010
 *      Author: nbryskin
 */

#include "session.hpp"
#include "proxy.hpp"

std::string session::default_http_port = std::string("80");

session::session(io_service& io, proxy& parent_proxy)
    : parent_proxy(parent_proxy), requester(io), responder(io), resolver(io)
    , request_channel(requester, responder, this)
    , response_channel(responder, requester, this)
    , opened_channels(0)
    , log(logging::keywords::channel = "session")
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
        parent_proxy.finished_session(shared_from_this(), ec);
}

void session::start_receive_header()
{
    requester.async_receive(buffer(header), requester.message_peek, boost::bind(&session::finished_receive_header, this,
            placeholders::error, placeholders::bytes_transferred));
}

void session::finished_receive_header(const error_code& ec, std::size_t bytes_transferred)
{
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);
    std::pair<std::string, std::string> peer = parse_header(bytes_transferred);
    start_resolving(peer);
}

void session::start_resolving(const std::pair<std::string, std::string>& peer)
{
    TRACE() << peer.first << ":" << peer.second;
    resolver.async_resolve(ip::tcp::resolver::query(peer.first, peer.second), boost::bind(&session::finished_resolving, this,
            placeholders::error, placeholders::iterator));
}

void session::finished_resolving(const error_code& ec, ip::tcp::resolver::iterator iterator)
{
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);
    start_connecting_to_peer(iterator);
}

void session::start_connecting_to_peer(ip::tcp::resolver::iterator iterator)
{
    TRACE() << iterator->endpoint();
    responder.async_connect(iterator->endpoint(), boost::bind(&session::finished_connecting_to_peer, this, placeholders::error));
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

std::pair<std::string, std::string> session::parse_header(std::size_t size)
{
    char* begin = std::find(header.begin(), header.begin() + size, ' ') + 8;
    char* end = std::find(begin, header.begin() + size, '/');
    char* colon = std::find(begin, end, ':');
    return colon == end ? std::make_pair(std::string(begin, colon), default_http_port) : std::make_pair(std::string(begin, colon), std::string(colon + 1, end));
}
