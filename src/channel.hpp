/*
 * channel.hpp
 *
 *  Created on: May 24, 2010
 *      Author: nbryskin
 */

#ifndef CHANNEL_HPP_
#define CHANNEL_HPP_

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/system/linux_error.hpp>

using namespace boost::asio;

class session;

class channel : public boost::noncopyable
{
public:
    channel(ip::tcp::socket& input, ip::tcp::socket& output, session* parent_session);

    void start();

    void start_waiting_input();

    void start_waiting_output();

    void finished_waiting_input(const boost::system::error_code& ec);

    void finished_waiting_output(const boost::system::error_code& ec);

    void splice_from_input();

    void splice_to_output();

    void finished_splice();

    void finish(const boost::system::error_code& ec);

protected:
    void splice(int from, int to, long& spliced, boost::system::error_code& ec);

private:
    ip::tcp::socket& input;
    ip::tcp::socket& output;
    int pipe[2];
    std::size_t pipe_size;
    bool waiting_input;
    bool waiting_output;
    session* parent_session;
};

#endif /* CHANNEL_HPP_ */
