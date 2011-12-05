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
#include <unbound.h>

#include "common.hpp"

using boost::system::error_code;

class resolver
{
public:
    class iterator
    {
    public:
        iterator(char** ptr=0)
        : ptr(reinterpret_cast<ip::address_v4**>(ptr))
        {
        }
        ip::address_v4& operator * ()
        {
            return **ptr;
        }
        iterator& operator ++ ()
        {
            ptr++;
            return *this;
        }
        operator bool ()
        {
            return *ptr;
        }
    private:
        ip::address_v4** ptr;
    };
    typedef boost::function<void (const boost::system::error_code&, iterator, iterator)> callback;

    resolver(asio::io_service& io, const ip::udp::endpoint& outbound);
    ~resolver();

    void start();

    int async_resolve(const char* host_name, const callback& completion);
    int cancel(int asyncid);

protected:
    void start_waiting_receive();
    void finished_waiting_receive(const boost::system::error_code& ec);

    static void finished_resolve_raw(void* data, int status, ub_result* result);
    static void finished_resolve(int status, ub_result* result, const callback& completion);

private:
    ip::udp::socket socket;
    ub_ctx* context;
    static logger log;
};

#endif /* RESOLVER_HPP_ */
