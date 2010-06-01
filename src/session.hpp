/*
 * session.hpp
 *
 *  Created on: May 24, 2010
 *      Author: nbryskin
 */

#ifndef SESSION_HPP_
#define SESSION_HPP_

#include <cstdint>
#include <boost/smart_ptr.hpp>
#include <boost/function.hpp>

#include "channel.hpp"
#include "resolver.hpp"
#include "common.hpp"

class proxy;

class session : public boost::noncopyable
{
public:
    session(asio::io_service& io, proxy& parent_proxy);

    ip::tcp::socket& socket();

    // called by proxy (parent)
    void start();

    // called by channel (child)
    void finish(const error_code& ec);

protected:
    void start_receive_header();
    void finished_receive_header(const error_code& ec, std::size_t bytes_transferred);

    void start_resolving(const char* peer);
    void finished_resolving(const error_code& ec, resolver::const_iterator begin, resolver::const_iterator end);

    void start_connecting_to_peer(const ip::tcp::endpoint& peer);
    void finished_connecting_to_peer(const error_code& ec);

    const char* parse_header(std::size_t size);

private:
    proxy& parent_proxy;
    ip::tcp::socket requester;
    ip::tcp::socket responder;
    ip::tcp::resolver resolver;
    channel request_channel;
    channel response_channel;
    const static std::size_t http_header_head_max_size = sizeof("GET http://") + 256;
    boost::array<char, http_header_head_max_size> header;
    const static std::uint16_t default_http_port = 80;
    int opened_channels;
    boost::function<void (const error_code&, resolver::const_iterator, resolver::const_iterator)> resolve_handler;
    std::uint16_t port;
    boost::timer timer;
    error_code prev_ec;
    static logger log;
};

typedef boost::shared_ptr<session> session_ptr;

#endif /* SESSION_HPP_ */
