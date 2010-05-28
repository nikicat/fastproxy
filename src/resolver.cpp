/*
 * resolver.cpp
 *
 *  Created on: May 27, 2010
 *      Author: nbryskin
 */

#include <boost/function.hpp>
#include <boost/log/sources/channel_feature.hpp>
#include "resolver.hpp"

void init_resolver()
{
    dns_init(0, 0);
}

resolver::resolver(io_service& io, const ip::udp::endpoint& outbound, const ip::udp::endpoint& name_server)
    : socket(io)
    , timer(io)
    , context(dns_new(0))
    , log(logging::keywords::channel = "proxy")
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

void resolver::async_resolve(const std::string& host_name, const callback& completion)
{
    TRACE() << host_name;
    void* data = new resolve_callback_internal(boost::bind(&resolver::finished_resolve, this, _1, _2, completion));
    dns_query* query = dns_submit_p(context, host_name.c_str(), DNS_C_IN, DNS_T_A, 0, dns_parse_a4, &resolver::finished_resolve_raw, data);
    if (query == 0)
        completion(boost::system::error_code(dns_status(context), boost::system::get_generic_category()), 0, 0);
    start_waiting_timer();
}

void resolver::start_waiting_receive()
{
    TRACE();
    socket.async_receive(null_buffers(), boost::bind(&resolver::finished_waiting_receive, this, placeholders::error));
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
    timer.expires_from_now(deadline_timer::duration_type(0, 0, seconds));
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
    resolve_callback_internal& completion = *static_cast<resolve_callback_internal*>(data);
    dns_rr_a4& response = *static_cast<dns_rr_a4*>(result);

    int status = dns_status(ctx);
    completion(status, response);
}

void resolver::finished_resolve(int status, const dns_rr_a4& response, const callback& completion)
{
    TRACE() << status;
    completion(boost::system::error_code(status, boost::system::get_generic_category()), reinterpret_cast<ip::address_v4*>(response.dnsa4_addr), reinterpret_cast<ip::address_v4*>(response.dnsa4_addr) + response.dnsa4_nrr);
}
