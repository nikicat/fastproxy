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

struct ub_create_error: std::exception { char const* what() const throw() { return "failed to create unbound context"; } };

resolver::resolver(asio::io_service& io, const ip::udp::endpoint& outbound)
    : socket(io)
    , context(ub_ctx_create())
{
	if(!context)
		throw ub_create_error();

	ub_ctx_set_option(context, const_cast<char*>("outgoing-interface:"), const_cast<char*>(outbound.address().to_string().c_str()));
	int fd = ub_fd(context);
	socket.assign(ip::udp::v4(), fd);
}

resolver::~resolver()
{
    ub_ctx_delete(context);
}

void resolver::start()
{
    start_waiting_receive();
}

int resolver::async_resolve(const char* host_name, const callback& completion)
{
    TRACE() << host_name;
	int asyncid = 0;
	int retval = ub_resolve_async(context, const_cast<char*>(host_name),
		1 /* TYPE A (IPv4 address) */, 
		1 /* CLASS IN (internet) */, 
		const_cast<callback*>(&completion), &resolver::finished_resolve_raw, &asyncid);
	if(retval != 0)
        completion(boost::system::error_code(retval, boost::system::get_generic_category()), 0, 0);
    return asyncid;
}

int resolver::cancel(int asyncid)
{
    return ub_cancel(context, asyncid);
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

    ub_process(context);

    start_waiting_receive();
}

void resolver::finished_resolve_raw(void* data, int status, ub_result* result)
{
    const callback& completion = *static_cast<const callback*>(data);

    finished_resolve(status, result, completion);
	ub_resolve_free(result);
}

void resolver::finished_resolve(int status, ub_result* result, const callback& completion)
{
    TRACE() << status;
	iterator begin, end;
	boost::system::error_code ec;
	if (status == 0)
	{
		if (result->havedata)
		{
			begin = result->data;
			for (end = begin; end; ++end);
		}
		else
		{
			ec = boost::system::error_code(result->rcode, boost::system::get_generic_category());
		}
	}
	else
	{
		ec = boost::system::error_code(status, boost::system::get_generic_category());
	}
    completion(ec, begin, end);
}
