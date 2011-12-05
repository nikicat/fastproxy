/*
 * proxy.hpp
 *
 *  Created on: May 27, 2010
 *      Author: nbryskin
 */

#ifndef PROXY_HPP_
#define PROXY_HPP_

#include <boost/ptr_container/ptr_set.hpp>
#include <boost/asio.hpp>

#include "common.hpp"
#include "resolver.hpp"
#include "session.hpp"
#include "headers.hpp"

class proxy : public boost::noncopyable
{
public:
    proxy(asio::io_service& io, std::vector<ip::tcp::endpoint> inbound, const ip::tcp::endpoint& outbound_http,
          const ip::udp::endpoint& outbound_ns,
          const time_duration& receive_timeout, const time_duration& connect_timeout, 
		  const time_duration& resolve_timeout, const std::vector<std::string>& allowed_headers,
          std::string error_pages_dir);

    // called by main (parent)
    void start();

    // called by session (child)
    resolver& get_resolver();

    // called by session (child)
    void finished_session(session* session, const boost::system::error_code& ec);

    const ip::tcp::endpoint& get_outgoing_endpoint() const;
    const time_duration& get_receive_timeout() const;
    const time_duration& get_connect_timeout() const;
    const time_duration& get_resolve_timeout() const;

    void dump_channels_state() const;

    const headers_type& get_allowed_headers() const;

    asio::const_buffer get_error_page(http_error_code httpec) const;

protected:
    void start_accept(ip::tcp::acceptor& acceptor);

    void handle_accept(const boost::system::error_code& ec, session* new_session, ip::tcp::acceptor& acceptor);
    void start_session(session* new_session);

private:
    typedef boost::ptr_set<session, std::pointer_to_binary_function<const session&, const session&, bool> > session_cont;
    typedef std::vector<boost::shared_ptr<ip::tcp::acceptor> > acceptor_vec;
    acceptor_vec acceptors;
    resolver resolver_;
    ip::tcp::endpoint outbound_http;
    time_duration receive_timeout;
    time_duration connect_timeout;
    time_duration resolve_timeout;
    session_cont sessions;
    headers_type allowed_headers;
    std::vector<std::string> headers_cont;
    std::vector<char> error_pages[HTTP_END - HTTP_BEGIN];
    static logger log;
};

#endif /* PROXY_HPP_ */
