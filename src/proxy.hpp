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

class proxy : public boost::noncopyable
{
public:
    proxy(io_service& io, ip::tcp::endpoint inbound)
        : acceptor(io, inbound)
        , log(logging::keywords::channel = "proxy")
    {
        TRACE() << "start listening on " << inbound;
        acceptor.listen();
    }

    void start_accept()
    {
        session_ptr new_sess(new session(acceptor.io_service(), *this));
        acceptor.async_accept(new_sess->socket(), boost::bind(&proxy::handle_accept, this, placeholders::error, new_sess));
    }

    void handle_accept(const boost::system::error_code& ec, session_ptr new_session)
    {
        TRACE_ERROR(ec);
        if (ec)
            return acceptor.io_service().stop();

        start_session(new_session);
    }

    void start_session(session_ptr new_session)
    {
        sessions.insert(new_session);
        new_session->start();
        start_accept();
    }

    void finished_session(session_ptr session, const boost::system::error_code& ec)
    {
        TRACE_ERROR(ec);
        sessions.erase(session);
    }

private:
    ip::tcp::acceptor acceptor;
    std::set<session_ptr> sessions;
    logging::sources::channel_logger<> log;
};

#endif /* PROXY_HPP_ */
