/*
 * signal.hpp
 *
 *  Created on: Jul 13, 2010
 *      Author: nbryskin
 */

#ifndef SIGNAL_HPP_
#define SIGNAL_HPP_

#include <map>
#include <unistd.h>
#include <boost/bind.hpp>
#include <boost/asio.hpp>

#include "common.hpp"

class signal_waiter
{
public:
    signal_waiter(asio::io_service& io)
        : write_sd(io)
        , read_sd(io)
    {
        int res = pipe(fds);
        if (res == -1)
        {
            boost::system::system_error e(boost::system::error_code(errno, boost::system::get_system_category()), "signal_waiter: pipe failed");
            boost::throw_exception(e);
        }

        write_sd.assign(fds[1]);
        read_sd.assign(fds[0]);
        instance = this;
    }

    ~signal_waiter()
    {
        for (sigactions_t::iterator it = sigactions.begin(); it != sigactions.end(); ++it)
            sigaction(it->first, &it->second, 0);
    }

    void add_signal(int signal, int flags = 0, sigset_t* mask = 0)
    {
        struct sigaction act, oldact;

        std::memset(&act, 0, sizeof(struct sigaction));
        act.sa_handler = signal_waiter::signal_handler;
        act.sa_flags = flags;
        if (mask)
            act.sa_mask = *mask;

        int res = sigaction(signal, &act, &oldact);
        if (res == -1)
        {
            boost::system::system_error e(boost::system::error_code(errno, boost::system::get_system_category()), "boost::asio::posix::basic_signal_handler_service::add_signal: sigaction failed");
            boost::throw_exception(e);
        }

        sigactions.insert(sigactions_t::value_type(signal, oldact));
    }

    void remove_signal(int signal)
    {
        sigactions_t::iterator it = sigactions.find(signal);
        if (it != sigactions.end())
        {
            sigaction(signal, &it->second, 0);
            sigactions.erase(it);
        }
    }

    typedef boost::function<void (const error_code&, int)> signal_handler_type;

    template<typename Handler>
    void async_wait(Handler handler)
    {
        asio::async_read(read_sd, asio::mutable_buffers_1(signal_buffer, sizeof signal_buffer), boost::bind(handler, placeholders::error(), signal_buffer[0]));
    }

private:
    static void signal_handler(int signal)
    {
        instance->write_sd.write_some(asio::const_buffers_1(&signal, sizeof signal));
    }

    int fds[2];
    asio::posix::stream_descriptor write_sd;
    asio::posix::stream_descriptor read_sd;
    int signal_buffer[1];
    typedef std::map<int, struct sigaction> sigactions_t;
    sigactions_t sigactions;
    static signal_waiter* instance;
};

#endif /* SIGNAL_HPP_ */
