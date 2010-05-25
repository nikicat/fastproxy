/*
 * session.hpp
 *
 *  Created on: May 24, 2010
 *      Author: nbryskin
 */

#ifndef SESSION_HPP_
#define SESSION_HPP_

#include <iostream>
#include <boost/function.hpp>
#include <boost/log/sources/channel_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include "channel.hpp"
#include "common.hpp"

using namespace boost::asio;
using boost::system::system_error;
using boost::system::error_code;
namespace logging = boost::log;

typedef boost::function<void(const boost::system::error_code&)> CompletionHandler;

class session : public boost::noncopyable
{
public:
    session(io_service& io)
        : requester(io), responder(io), resolver(io)
        , request_channel(requester, responder, this)
        , response_channel(responder, requester, this)
        , opened_channels(0)
        , log(logging::keywords::channel = "session")
    {
    }

    ip::tcp::socket& socket()
    {
        return requester;
    }

    void start(const CompletionHandler& completion)
    {
        this->completion = completion;
        start_receive_header();
    }

    void finish(const error_code& ec)
    {
        opened_channels--;
        TRACE_ERROR(ec);
        if (opened_channels == 0)
            completion(ec);
    }

    void start_receive_header()
    {
        requester.async_receive(buffer(header), requester.message_peek, boost::bind(&session::finished_receive_header, this,
                placeholders::error, placeholders::bytes_transferred));
    }

    void finished_receive_header(const error_code& ec, std::size_t bytes_transferred)
    {
        TRACE_ERROR(ec);
        if (ec)
            return finish(ec);
        std::pair<std::string, std::string> peer = parse_header(bytes_transferred);
        start_resolving(peer);
    }

    void start_resolving(const std::pair<std::string, std::string>& peer)
    {
        TRACE() << peer.first << ":" << peer.second;
        resolver.async_resolve(ip::tcp::resolver::query(peer.first, peer.second), boost::bind(&session::finished_resolving, this,
                placeholders::error, placeholders::iterator));
    }

    void finished_resolving(const error_code& ec, ip::tcp::resolver::iterator iterator)
    {
        TRACE_ERROR(ec);
        if (ec)
            return finish(ec);
        start_connecting_to_peer(iterator);
    }

    void start_connecting_to_peer(ip::tcp::resolver::iterator iterator)
    {
        TRACE() << iterator->endpoint();
        responder.async_connect(iterator->endpoint(), boost::bind(&session::finished_connecting_to_peer, this, placeholders::error));
    }

    void finished_connecting_to_peer(const error_code& ec)
    {
        TRACE_ERROR(ec);
        if (ec)
            return finish(ec);
        opened_channels++;
        request_channel.start();
        opened_channels++;
        response_channel.start();
    }

protected:
    std::pair<std::string, std::string> parse_header(std::size_t size)
    {
        char* begin = std::find(header.begin(), header.begin() + size, ' ') + 8;
        char* end = std::find(begin, header.begin() + size, '/');
        char* colon = std::find(begin, end, ':');
        return colon == end ? std::make_pair(std::string(begin, colon), default_http_port) : std::make_pair(std::string(begin, colon), std::string(colon + 1, end));
    }

private:
    ip::tcp::socket requester;
    ip::tcp::socket responder;
    ip::tcp::resolver resolver;
    channel request_channel;
    channel response_channel;
    CompletionHandler completion;
    const static std::size_t http_header_head_max_size = sizeof("GET http://") + 256;
    boost::array<char, http_header_head_max_size> header;
    static std::string default_http_port;
    int opened_channels;
    logging::sources::channel_logger<> log;
};

typedef boost::shared_ptr<session> session_ptr;

#endif /* SESSION_HPP_ */
