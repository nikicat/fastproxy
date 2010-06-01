/*
 * proxy.hpp
 *
 *  Created on: May 27, 2010
 *      Author: nbryskin
 */

#ifndef PROXY_HPP_
#define PROXY_HPP_

#include <set>
#include <boost/asio.hpp>

#include "common.hpp"
#include "resolver.hpp"

class session;

class proxy : public boost::noncopyable
{
public:
    proxy(asio::io_service& io, const ip::tcp::endpoint& inbound, const ip::udp::endpoint& outbound, const ip::udp::endpoint& name_server);

    // called by main (parent)
    void start();

    // called by session (child)
    resolver& get_resolver();

    // called by session (child)
    void finished_session(session* session, const boost::system::error_code& ec);

protected:
    void start_accept();

    void handle_accept(const boost::system::error_code& ec, session* new_session);
    void start_session(session* new_session);

    void start_waiting_dump_statistics();
    void finished_waiting_dump_statistics(const error_code& ec);

private:
    typedef std::set<session*> session_cont;
    ip::tcp::acceptor acceptor;
    resolver resolver_;
    session_cont sessions;
    asio::deadline_timer timer;
    static const std::size_t dump_interval = 3;
    static logger log;
};

#endif /* PROXY_HPP_ */
