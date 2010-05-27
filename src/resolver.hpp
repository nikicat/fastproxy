/*
 * resolver.hpp
 *
 *  Created on: May 27, 2010
 *      Author: nbryskin
 */

#ifndef RESOLVER_HPP_
#define RESOLVER_HPP_

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
    typedef in_addr* iterator;
    typedef const in_addr* const_iterator;
    typedef boost::function<void (const boost::system::error_code&, const_iterator, const_iterator)> callback;

    resolver(io_service& io, const ip::udp::endpoint& outbound, const ip::udp::endpoint& name_server);

    ~resolver();

    void start();

    void async_resolve(const std::string& host_name, const callback& completion);

protected:
    void start_receiving_resolve_data();

    void finish_receiving_resolve_data(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/);

    static void finished_resolve_raw(dns_ctx* ctx, void* result, void* data);

    void finished_resolve(int status, const dns_rr_a4& response, const callback& completion);

private:
    typedef boost::function<void (int, const dns_rr_a4&)> resolve_callback_internal;
    ip::udp::socket socket;
    dns_ctx* context;
    logging::sources::channel_logger<> log;
};

#endif /* RESOLVER_HPP_ */
