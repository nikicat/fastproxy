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
#include <boost/bind.hpp>

#include "common.hpp"
#include "session.hpp"
#include "resolver.hpp"

class proxy : public boost::noncopyable
{
public:
    proxy(io_service& io, const ip::tcp::endpoint& inbound, const ip::udp::endpoint& outbound, const ip::udp::endpoint& name_server)
        : acceptor(io, inbound)
        , resolver_(io, outbound, name_server)
        , log(logging::keywords::channel = "proxy")
    {
        TRACE() << "start listening on " << inbound;
        acceptor.listen();
    }

    // called by main (parent)
    void start()
    {
        start_accept();
        resolver_.start();
    }

    // called by session (child)
    resolver& get_resolver()
    {
        return resolver_;
    }

    // called by session (child)
    void finished_session(session* session, const boost::system::error_code& ec)
    {
        TRACE_ERROR(ec);
        sessions.erase(session);
        delete session;
    }

protected:
    void start_accept()
    {
        std::unique_ptr<session> new_sess(new session(acceptor.io_service(), *this));
        acceptor.async_accept(new_sess->socket(), boost::bind(&proxy::handle_accept, this, placeholders::error, new_sess.get()));
        new_sess.release();
    }

    void handle_accept(const boost::system::error_code& ec, session* new_session)
    {
        std::unique_ptr<session> session_ptr(new_session);
        TRACE_ERROR(ec);
        if (ec)
            return;

        start_session(session_ptr.get());
        session_ptr.release();
    }

    void start_session(session* new_session)
    {
        sessions.insert(new_session);
        new_session->start();
        start_accept();
    }

private:
    typedef std::set<session*> session_cont;
    ip::tcp::acceptor acceptor;
    resolver resolver_;
    session_cont sessions;
    logging::sources::channel_logger<> log;
};

#endif /* PROXY_HPP_ */
