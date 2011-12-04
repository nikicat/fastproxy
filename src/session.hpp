/*
 * session.hpp
 *
 *  Created on: May 24, 2010
 *      Author: nbryskin
 */

#ifndef SESSION_HPP_
#define SESSION_HPP_

#include <cstdint>
#include <vector>
#include <boost/smart_ptr.hpp>
#include <boost/function.hpp>

#include "channel.hpp"
#include "resolver.hpp"
#include "common.hpp"
#include "high_resolution_timer.hpp"

class proxy;

class session : public boost::noncopyable
{
public:
    session(asio::io_service& io, proxy& parent_proxy);

    ip::tcp::socket& socket();

    // called by proxy (parent)
    void start();

    // called by channel (child)
    void finished_channel(const error_code& ec);

    const channel& get_request_channel() const;
    const channel& get_response_channel() const;
    int get_opened_channels() const;
    const void* get_id() const;

protected:
    void start_receive_header();
    void finished_receive_header(const error_code& ec, std::size_t bytes_transferred);

    void start_resolving(const char* peer);
    void finished_resolving(const error_code& ec, resolver::const_iterator begin, resolver::const_iterator end);

    void start_waiting_connect_timer();
    void finished_waiting_connect_timer(const error_code& ec);

    void start_sending_error(http_error_code httpec);
    void finished_sending_error(const error_code& ec, std::size_t bytes_transferred);

    void start_connecting_to_peer(const ip::tcp::endpoint& peer);
    void finished_connecting_to_peer(const error_code& ec);

    void start_sending_header();
    void finished_sending_header(const error_code& ec);

    void start_sending_connect_response();
    void finished_sending_connect_response(const error_code& ec);

    void start_channels();

    void finish(const error_code& ec);

    const char* parse_header(std::size_t size);
    void prepare_header();
    void filter_headers();

private:
    friend class channel;

    const static std::size_t http_header_head_max_size = 4096;
    const static std::uint16_t default_http_port = 80;

    enum method_type
    {
        CONNECT,
        OTHER,
    };

    proxy& parent_proxy;
    ip::tcp::socket requester;
    ip::tcp::socket responder;
    channel request_channel;
    channel response_channel;
    // header info
    boost::array<char, http_header_head_max_size> header_data;
    std::uint16_t port;
    std::vector<asio::const_buffer> output_headers;
    asio::const_buffer headers_tail;
    method_type method;

    int opened_channels;
    boost::function<void (const error_code&, resolver::const_iterator, resolver::const_iterator)> resolve_handler;
    util::high_resolution_timer timer;
    error_code prev_ec;
    static logger log;
    const time_duration connect_timeout;
    asio::deadline_timer connect_timer;
};

typedef boost::shared_ptr<session> session_ptr;

#endif /* SESSION_HPP_ */
