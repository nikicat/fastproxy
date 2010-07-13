/*
 * resolver.cpp
 *
 *  Created on: May 27, 2010
 *      Author: nbryskin
 */

#include <boost/function.hpp>
#include <boost/log/sources/channel_feature.hpp>
#include "resolver.hpp"

logger resolver::log = logger(keywords::channel = "resolver");

void resolver::init()
{
    dns_init(0, 0);
}

resolver::resolver(asio::io_service& io, const ip::udp::endpoint& outbound, const ip::udp::endpoint& name_server)
    : socket(io)
    , timer(io)
    , context(dns_new(0))
{
    dns_add_serv_s(context, 0);
    dns_add_serv_s(context, name_server.data());
    socket.assign(ip::udp::v4(), dns_open(context));
    socket.bind(outbound);
    socket.connect(name_server);
}

resolver::~resolver()
{
    dns_free(context);
}

void resolver::start()
{
    start_waiting_receive();
}

void resolver::async_resolve(const char* host_name, const callback& completion)
{
    TRACE() << host_name;
    dns_query* query = dns_submit_p(context, host_name, DNS_C_IN, DNS_T_A, 0, dns_parse_a4, &resolver::finished_resolve_raw, const_cast<callback*>(&completion));
    if (query == 0)
        completion(boost::system::error_code(dns_status(context), boost::system::get_generic_category()), 0, 0);
    start_waiting_timer();
}

void resolver::start_waiting_receive()
{
    TRACE();
    socket.async_receive(asio::null_buffers(), boost::bind(&resolver::finished_waiting_receive, this, placeholders::error));
}

void resolver::finished_waiting_receive(const boost::system::error_code& ec)
{
    TRACE_ERROR(ec);
    if (ec)
        return;

    dns_ioevent(context, 0);

    start_waiting_receive();
    start_waiting_timer();
}

void resolver::start_waiting_timer()
{
    int seconds = dns_timeouts(context, -1, 0);
    TRACE() << seconds;
    if (seconds < 0)
        return;
    timer.expires_from_now(asio::deadline_timer::duration_type(0, 0, seconds));
    timer.async_wait(boost::bind(&resolver::finished_waiting_timer, this, placeholders::error));
}

void resolver::finished_waiting_timer(const error_code& ec)
{
    TRACE_ERROR(ec);
    if (ec)
        return;

    dns_ioevent(context, 0);

    start_waiting_timer();
}

void resolver::finished_resolve_raw(dns_ctx* ctx, void* result, void* data)
{
    const callback& completion = *static_cast<const callback*>(data);
    dns_rr_a4& response = *static_cast<dns_rr_a4*>(result);

    int status = dns_status(ctx);
    finished_resolve(status, response, completion);

    free(result);
}

void resolver::finished_resolve(int status, const dns_rr_a4& response, const callback& completion)
{
    TRACE() << status;
    completion(boost::system::error_code(status, boost::system::get_generic_category()),
            status < 0 ? 0 : reinterpret_cast<ip::address_v4*>(response.dnsa4_addr),
            status < 0 ? 0 : reinterpret_cast<ip::address_v4*>(response.dnsa4_addr) + response.dnsa4_nrr);
}
