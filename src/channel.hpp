/*
 * channel.hpp
 *
 *  Created on: May 24, 2010
 *      Author: nbryskin
 */

#ifndef CHANNEL_HPP_
#define CHANNEL_HPP_

#include <boost/function.hpp>
#include "common.hpp"

typedef boost::function<void (const error_code&, std::size_t)> handler_t;

void* asio_handler_allocate(std::size_t s, handler_t** h);
void asio_handler_deallocate(void* pointer, std::size_t size, handler_t** h);
template <typename Function>
void asio_handler_invoke(Function function, handler_t** h);

#include <boost/asio.hpp>
#include <boost/utility.hpp>

using namespace boost::asio;
using boost::system::error_code;

class session;

class channel : public boost::noncopyable
{
public:
    channel(ip::tcp::socket& input, ip::tcp::socket& output, session* parent_session);
    ~channel();

    void start();

protected:
    void start_waiting();
    void start_waiting_input();
    void start_waiting_output();

    void finished_waiting_input(const error_code& ec, std::size_t);
    void finished_waiting_output(const error_code& ec, std::size_t);

    void splice_from_input();
    void splice_to_output();

    void finished_splice();
    void finish(const error_code& ec);

    void splice(int from, int to, long& spliced, error_code& ec);

private:
    ip::tcp::socket& input;
    ip::tcp::socket& output;
    int pipe[2];
    long pipe_size;
    session* parent_session;
    static channel_logger log;
    static const std::size_t size_of_operation = sizeof(detail::null_buffers_op<handler_t*>);
    handler_t input_handler;
    char space_for_input_op[size_of_operation];
    handler_t output_handler;
    char space_for_output_op[size_of_operation];
};

#endif /* CHANNEL_HPP_ */
