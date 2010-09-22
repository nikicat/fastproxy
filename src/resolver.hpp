/*
 * resolver.hpp
 *
 *  Created on: May 27, 2010
 *      Author: nbryskin
 */

#ifndef RESOLVER_HPP_
#define RESOLVER_HPP_

#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/bind/placeholders.hpp>
#include <boost/log/sources/channel_logger.hpp>
#include <udns.h>

#include "common.hpp"

using boost::system::error_code;

class resolver
{
public:
    typedef ip::address_v4* iterator;
    typedef const ip::address_v4* const_iterator;
    typedef boost::function<void (const boost::system::error_code&, const_iterator, const_iterator)> callback;

    static void init();

    resolver(asio::io_service& io, const ip::udp::endpoint& outbound, const ip::udp::endpoint& name_server);
    ~resolver();

    void start();
    void async_resolve(const char* host_name, const callback& completion);

protected:
    void start_waiting_receive();
    void finished_waiting_receive(const boost::system::error_code& ec);

    void start_waiting_timer();
    void finished_waiting_timer(const error_code& ec);

    static void finished_resolve_raw(dns_ctx* ctx, void* result, void* data);
    static void finished_resolve(int status, const dns_rr_a4& response, const callback& completion);

private:
    typedef boost::function<void (int, const dns_rr_a4&)> resolve_callback_internal;
    ip::udp::socket socket;
    asio::deadline_timer timer;
    dns_ctx* context;
    static logger log;
};

#endif /* RESOLVER_HPP_ */
