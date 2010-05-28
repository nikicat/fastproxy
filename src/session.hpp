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
#include <boost/log/sources/channel_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include "channel.hpp"
#include "resolver.hpp"
#include "common.hpp"

class proxy;

class session : public boost::noncopyable
{
public:
    session(io_service& io, proxy& parent_proxy);

    ip::tcp::socket& socket();

    // called by proxy (parent)
    void start();

    // called by channel (child)
    void finish(const error_code& ec);

protected:
    void start_receive_header();
    void finished_receive_header(const error_code& ec, std::size_t bytes_transferred);

    void start_resolving(const std::pair<std::string, std::uint16_t>& peer);
    void finished_resolving(const error_code& ec, resolver::const_iterator begin, resolver::const_iterator end, std::uint16_t port);

    void start_connecting_to_peer(const ip::tcp::endpoint& peer);
    void finished_connecting_to_peer(const error_code& ec);

    std::pair<std::string, std::uint16_t> parse_header(std::size_t size);

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
    logging::sources::channel_logger<> log;
};

typedef boost::shared_ptr<session> session_ptr;

#endif /* SESSION_HPP_ */
