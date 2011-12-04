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
#include "headers.hpp"

logger session::log = logger(keywords::channel = "session");

session::session(asio::io_service& io, proxy& parent_proxy)
    : parent_proxy(parent_proxy), requester(io), responder(io)
    , request_channel(requester, responder, *this, parent_proxy.get_receive_timeout())
    , response_channel(responder, requester, *this, parent_proxy.get_receive_timeout(), /*first_input_stat=*/true)
    , opened_channels(2)
    , resolve_handler(boost::bind(&session::finished_resolving, this, placeholders::error(), _2, _3))
    , connect_timeout(parent_proxy.get_connect_timeout())
    , connect_timer(io)
{
}

ip::tcp::socket& session::socket()
{
    return requester;
}

void session::start()
{
    timer.restart();
    statistics::increment("total_sessions");
    statistics::increment("current_sessions");
    requester.set_option(asio::ip::tcp::no_delay(true));
    start_receive_header();
}

void session::finished_channel(const error_code& ec)
{
    TRACE_ERROR(ec) << get_id();
    if (--opened_channels == 0)
    {
        finish((ec && ec != asio::error::operation_aborted && ec != asio::error::eof) ? ec : prev_ec);
    }
    else
    {
        prev_ec = ec;
        if (ec)
        {
            error_code tmp_ec;
            requester.close(tmp_ec);
            responder.close(tmp_ec);
            requester.cancel(tmp_ec);
            responder.cancel(tmp_ec);
        }
        statistics::increment("channel_time", timer.elapsed());
    }
}

void session::finish(const error_code& ec)
{
    statistics::increment("session_time", timer.elapsed());
    statistics::decrement("current_sessions");
    statistics::increment("finished_sessions");
    if (ec && ec != asio::error::eof)
    {
        statistics::increment("failed_sessions");
        BOOST_LOG_SEV(log, severity_level::error) << system_error(ec).what();
    }
    parent_proxy.finished_session(this, ec);
}

void session::start_receive_header()
{
    requester.async_receive(asio::buffer(header_data), boost::bind(&session::finished_receive_header, this,
            placeholders::error(), placeholders::bytes_transferred));
}

void session::finished_receive_header(const error_code& ec, std::size_t bytes_transferred)
{
    statistics::increment("request_header_time", timer.elapsed());
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);
    const char* dn = parse_header(bytes_transferred);
    error_code convert_ec;
    const ip::address& peer_addr = ip::address::from_string(dn, convert_ec);
    if (convert_ec)
        start_resolving(dn);
    else
        start_connecting_to_peer(ip::tcp::endpoint(peer_addr, port));
}

void session::start_resolving(const char* peer)
{
    TRACE() << peer << ":" << port;
    parent_proxy.get_resolver().async_resolve(peer, resolve_handler);
}

void session::finished_resolving(const error_code& ec, resolver::const_iterator begin, resolver::const_iterator end)
{
    TRACE_ERROR(ec);
    if (ec)
    {
        statistics::increment("resolve_failed");
        start_sending_error(HTTP_503);
        return;
    }
    statistics::increment("resolve_time", timer.elapsed());
    // TODO: cycle throw all addresses
    start_connecting_to_peer(ip::tcp::endpoint(*begin, port));
    start_waiting_connect_timer();
}

void session::start_waiting_connect_timer()
{
    TRACE();
    connect_timer.expires_from_now(asio::deadline_timer::duration_type(0, 0, connect_timeout.seconds()));
    connect_timer.async_wait(boost::bind(&session::finished_waiting_connect_timer, this, placeholders::error));
}

void session::finished_waiting_connect_timer(const error_code& ec)
{
    TRACE_ERROR(ec);
    if (ec)
        return;

    responder.cancel();
}

void session::start_sending_error(http_error_code httpec)
{
    requester.async_send(asio::const_buffers_1(parent_proxy.get_error_page(httpec)), boost::bind(&session::finished_sending_error, this, placeholders::error(), placeholders::bytes_transferred));
}

void session::finished_sending_error(const error_code& ec, std::size_t bytes_transferred)
{
    TRACE_ERROR(ec);
    if (ec)
        statistics::increment("send_error_failed");
    finish(ec);
}

void session::start_connecting_to_peer(const ip::tcp::endpoint& peer)
{
    TRACE() << peer;
    try
    {
        responder.open(peer.protocol());
        responder.bind(parent_proxy.get_outgoing_endpoint());
    }
    catch (const boost::system::system_error& e)
    {
        TRACE_ERROR(e.code());
        return finish(e.code());
    }
    responder.async_connect(peer, boost::bind(&session::finished_connecting_to_peer, this, placeholders::error()));
}

void session::finished_connecting_to_peer(const error_code& ec)
{
    TRACE_ERROR(ec);
    connect_timer.cancel();
    if (ec)
    {
        statistics::increment("connect_failed");
        start_sending_error(HTTP_504);
        return;
    }
    statistics::increment("connected_time", timer.elapsed());
    switch (method)
    {
        case CONNECT:
            start_sending_connect_response();
            break;

        default:
            start_sending_header();
    }
}

void session::start_sending_header()
{
    prepare_header();
    filter_headers();
    responder.async_send(output_headers, boost::bind(&session::finished_sending_header, this, placeholders::error()));
}

void session::finished_sending_header(const error_code& ec)
{
    statistics::increment("send_request_header_time", timer.elapsed());
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);
    start_channels();
}

void session::start_sending_connect_response()
{
    static const char ok_response[] = "HTTP/1.0 200 Connection established\r\n\r\n";
    requester.async_send(asio::const_buffers_1(ok_response, sizeof(ok_response) - 1), boost::bind(&session::finished_sending_connect_response, this, placeholders::error()));
}

void session::finished_sending_connect_response(const error_code& ec)
{
    statistics::increment("send_connect_response_time", timer.elapsed());
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);

    start_channels();
}

void session::start_channels()
{
    request_channel.start();
    response_channel.start();
}

const char* session::parse_header(std::size_t size)
{
    // it could be GET http://ya.ru HTTP/1.0
    //          or GET http://ya.ru/index.html HTTP/1.0
    using boost::lambda::_1;
    char* begin = header_data.begin();
    char* end = begin + size;
    char* method_end = std::find(begin, end, ' ');
    if (std::mismatch(begin, method_end, "CONNECT").first == method_end)
        method = CONNECT;
    else
        method = OTHER;
    char* url = method_end + 1;
    output_headers.push_back(asio::const_buffer(header_data.begin(), url - header_data.begin()));

    char* dn_begin = url + (method == CONNECT ? 0 : sizeof("http://") - 1);
    char* dn_end = std::find_if(dn_begin, end, _1 == ' ' || _1 == '/');
    char* colon = std::find(dn_begin, dn_end, ':');
    char* resource = *dn_end == '/' ? dn_end : dn_end - 1;
    // it should be GET http://ya.ru\0HTTP/1.0
    //                             ^-resource
    //           or GET http://ya.ru\0index.html HTTP/1.0
    //                              ^^-resource
    headers_tail = asio::const_buffer(resource, end - resource);

    // temporary replace first resource byte with zero (replace with / in prepare_header)
    *dn_end = 0;
    *colon = 0;
    // TODO: replace with c++-style conversion
    port = (colon == dn_end ? default_http_port : std::uint16_t(atoi(colon + 1)));
    return dn_begin;
}

void session::prepare_header()
{
    // it could be GET http://ya.ru\0HTTP/1.0
    //                            ^-resource
    //          or GET http://ya.ru\0index.html HTTP/1.0
    //                             ^^-resource
    char* resource = const_cast<char*>(asio::buffer_cast<const char*>(headers_tail));
    *resource = '/';
    if (*(resource + 1) == 0)
        *(resource + 1) = ' ';
}

lstring get_next_header(const lstring& headers, const lstring& current)
{
    if (*current.end == '\r' || *current.end == '\n')
        return lstring();
    const char* header = std::find(current.end, headers.end, '\n');
    if (header == headers.end)
        return lstring();
    else
        return lstring(current.end, header + 1);
}

void session::filter_headers()
{
    const headers_type& allowed_headers = parent_proxy.get_allowed_headers();
    if (allowed_headers.empty())
    {
        output_headers.push_back(headers_tail);
        return;
    }

    const lstring headers(asio::buffer_cast<const char*>(headers_tail), asio::buffer_cast<const char*>(headers_tail) + asio::buffer_size(headers_tail));
    lstring allowed = get_next_header(headers, lstring(headers.begin, headers.begin));
    if (!allowed)
    {
        output_headers.push_back(headers_tail);
        return;
    }

    for (;;)
    {
        const lstring& header = get_next_header(headers, allowed);
        if (!header)
            break;
        if (allowed_headers.find(header) == allowed_headers.end())
        {
            if (allowed)
                output_headers.push_back(asio::const_buffer(allowed.begin, allowed.size()));
            allowed.begin = header.end;
        }
        allowed.end = header.end;
    }
    allowed.end = headers.end;

    if (allowed)
        output_headers.push_back(asio::const_buffer(allowed.begin, allowed.size()));
}

const channel& session::get_request_channel() const
{
    return request_channel;
}

const channel& session::get_response_channel() const
{
    return response_channel;
}

int session::get_opened_channels() const
{
    return opened_channels;
}

const void* session::get_id() const
{
    return this;
}
