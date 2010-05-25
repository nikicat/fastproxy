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
#include <boost/log/sources/channel_logger.hpp>

using namespace boost::asio;
using boost::system::error_code;

class session;

class channel : public boost::noncopyable
{
public:
    channel(ip::tcp::socket& input, ip::tcp::socket& output, session* parent_session);

    void start();
    void start_waiting();
    void start_waiting_input();
    void start_waiting_output();

    void finished_waiting_input(const error_code& ec);
    void finished_waiting_output(const error_code& ec);

    void splice_from_input();
    void splice_to_output();

    void finished_splice();
    void finish(const error_code& ec);

protected:
    void splice(int from, int to, long& spliced, error_code& ec);

private:
    ip::tcp::socket& input;
    ip::tcp::socket& output;
    int pipe[2];
    long pipe_size;
    bool waiting_input;
    bool waiting_output;
    session* parent_session;
    boost::log::sources::channel_logger<> log;
};

#endif /* CHANNEL_HPP_ */
