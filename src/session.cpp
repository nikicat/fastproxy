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
    : parent_proxy(parent_proxy), requester(io), responder(io, parent_proxy.get_outgoing_endpoint())
    , request_channel(requester, responder, this, parent_proxy.get_receive_timeout())
    , response_channel(responder, requester, this, parent_proxy.get_receive_timeout())
    , opened_channels(2)
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

void session::finished_channel(const error_code& ec)
{
    opened_channels--;
    TRACE_ERROR(ec) << get_id();
    if (ec)
        BOOST_LOG_SEV(log, severity_level::error) << system_error(ec, "channel error").what();
    if (opened_channels == 0)
    {
        statistics::push("sesstm", timer.elapsed());
        finish(ec ? ec : prev_ec);
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
        statistics::push("chantm", timer.elapsed());
    }
}

void session::finish(const error_code& ec)
{
    parent_proxy.finished_session(this, ec);
}

void session::start_receive_header()
{
    timer.restart();
    requester.async_receive(asio::buffer(header_data), boost::bind(&session::finished_receive_header, this,
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
    timer.restart();
    responder.async_send(output_header, boost::bind(&session::finished_sending_header, this, placeholders::error()));
}

void session::finished_sending_header(const error_code& ec)
{
    statistics::push("sndhdrtm", timer.elapsed());
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);
    start_channels();
}

void session::start_sending_connect_response()
{
    static const char ok_response[] = "HTTP/1.0 200 Connection established\r\n\r\n";
    timer.restart();
    requester.async_send(asio::const_buffers_1(ok_response, sizeof(ok_response) - 1), boost::bind(&session::finished_sending_connect_response, this, placeholders::error()));
}

void session::finished_sending_connect_response(const error_code& ec)
{
    statistics::push("sndcrtm", timer.elapsed());
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);

    start_channels();
}

void session::start_channels()
{
    timer.restart();
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
    if (std::mismatch(begin, method_end, "GET").first == method_end)
        method = GET;
    else if (std::mismatch(begin, method_end, "CONNECT").first == method_end)
        method = CONNECT;
    else if (std::mismatch(begin, method_end, "POST").first == method_end)
        method = POST;
    char* url = method_end + 1;
    output_header[0] = asio::const_buffer(header_data.begin(), url - header_data.begin());

    char* dn_begin = url + (method == CONNECT ? 0 : sizeof("http://") - 1);
    char* dn_end = std::find_if(dn_begin, end, _1 == ' ' || _1 == '/');
    char* colon = std::find(dn_begin, dn_end, ':');
    char* resource = *dn_end == '/' ? dn_end : dn_end - 1;
    // it should be GET http://ya.ru\0HTTP/1.0
    //                             ^-resource
    //           or GET http://ya.ru\0index.html HTTP/1.0
    //                              ^^-resource
    output_header[1] = asio::const_buffer(resource, end - resource);

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
    char* resource = const_cast<char*>(asio::buffer_cast<const char*>(output_header[1]));
    *resource = '/';
    if (*(resource + 1) == 0)
        *(resource + 1) = ' ';
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
