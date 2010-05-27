/*
 * resolver.cpp
 *
 *  Created on: May 27, 2010
 *      Author: nbryskin
 */

#include <boost/function.hpp>
#include <boost/log/sources/channel_feature.hpp>
#include "resolver.hpp"

resolver::resolver(io_service& io, const ip::udp::endpoint& outbound, const ip::udp::endpoint& name_server)
    : socket(io)
    , context(dns_new(0))
    , log(logging::keywords::channel = "proxy")
{
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
    start_receiving_resolve_data();
}

void resolver::async_resolve(const std::string& host_name, const callback& completion)
{
    void* data = new resolve_callback_internal(boost::bind(&resolver::finished_resolve, this, _1, _2, completion));
    dns_submit_p(context, host_name.c_str(), DNS_C_IN, DNS_T_A, 0, 0, &resolver::finished_resolve_raw, data);
}

void resolver::start_receiving_resolve_data()
{
    socket.async_receive(null_buffers(), boost::bind(&resolver::finish_receiving_resolve_data, this, placeholders::error, placeholders::bytes_transferred));
}

void resolver::finish_receiving_resolve_data(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
{
    TRACE_ERROR(ec);
    if (ec)
        return;

    dns_ioevent(context, 0);
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
    completion(boost::system::error_code(status, boost::system::get_generic_category()), response.dnsa4_addr, response.dnsa4_addr + response.dnsa4_nrr);
}
