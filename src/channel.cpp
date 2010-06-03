/*
 * channel.cpp
 *
 *  Created on: May 24, 2010
 *      Author: nbryskin
 */

#include <iostream>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <boost/log/sources/channel_feature.hpp>

#include "channel.hpp"
#include "session.hpp"
#include "statistics.hpp"

using namespace boost::system;

const long PIPE_SIZE = 65536;

void* asio_handler_allocate(std::size_t s, handler_t** h)
{
    return *h + 1;
}

void asio_handler_deallocate(void* pointer, std::size_t size, handler_t** h)
{
}

template <typename Function>
void asio_handler_invoke(Function function, handler_t** h)
{
    (**h)(function.arg1_, function.arg2_);
}

logger channel::log = logger(keywords::channel = "channel");

channel::channel(ip::tcp::socket& input, ip::tcp::socket& output, session* parent_session)
    : input(input)
    , output(output)
    , pipe_size(0)
    , parent_session(parent_session)
    , input_handler(boost::bind(&channel::finished_waiting_input, this, placeholders::error(), placeholders::bytes_transferred()))
    , output_handler(boost::bind(&channel::finished_waiting_output,this, placeholders::error(), placeholders::bytes_transferred()))
    , splices_count(0)
    , current_state(created)
{
    if (pipe2(pipe, O_NONBLOCK) == -1)
    {
        perror("pipe2");
        throw boost::system::errc::make_error_code(static_cast<boost::system::errc::errc_t> (errno));
    }
}

channel::~channel()
{
    close(pipe[0]);
    close(pipe[1]);
}

void channel::start()
{
    start_waiting();
}

void channel::start_waiting_input()
{
    TRACE();
    current_state = waiting_input;
    input.async_read_some(asio::null_buffers(), &input_handler);
}

void channel::start_waiting_output()
{
    TRACE();
    current_state = waiting_output;
    output.async_write_some(asio::null_buffers(), &output_handler);
}

void channel::finished_waiting_input(const error_code& ec, std::size_t)
{
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);

    splice_from_input();
}

void channel::finished_waiting_output(const error_code& ec, std::size_t)
{
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);

    splice_to_output();
}

void channel::splice_from_input()
{
    if (!input.is_open())
    {
        TRACE() << "socket closed";
        return finish(error_code());
    }

    if (input.available() == 0)
    {
        TRACE() << "connection closed";
        return finish(error_code());
    }
    current_state = splicing_input;

    long spliced;
    boost::system::error_code ec;
    splice(input.native(), pipe[1], spliced, ec);
    assert(spliced >= 0);
    pipe_size += spliced;
    assert(pipe_size >= 0);
    if (ec)
        return finish(ec);

    finished_splice();
}

void channel::splice_to_output()
{
    if (!output.is_open())
    {
        TRACE() << "socket closed";
        return finish(error_code());
    }

    current_state = splicing_output;
    long spliced;
    boost::system::error_code ec;
    splice(pipe[0], output.native(), spliced, ec);
    assert(spliced >= 0);
    pipe_size -= spliced;
    assert(pipe_size >= 0);
    if (ec)
        return finish(ec);

    finished_splice();
}

void channel::finished_splice()
{
    start_waiting();
}

void channel::start_waiting()
{
    TRACE() << " pipe_size=" << pipe_size;

    if (pipe_size > 0)
        start_waiting_output();
    else if (pipe_size < PIPE_SIZE)
        start_waiting_input();
}

void channel::finish(const error_code& ec)
{
    TRACE_ERROR(ec) << parent_session->get_id();
    current_state = finished;
    statistics::push("splcnt", splices_count);
    parent_session->finished_channel(ec);
}

void channel::splice(int from, int to, long& spliced, error_code& ec)
{
    splices_count++;
    spliced = ::splice(from, 0, to, 0, PIPE_SIZE, SPLICE_F_NONBLOCK | SPLICE_F_MORE | MSG_NOSIGNAL);
    if (spliced == -1)
    {
        ec = boost::system::errc::make_error_code(static_cast<boost::system::errc::errc_t> (errno));
        TRACE_ERROR(ec);
        spliced = 0;

        if (ec == boost::system::errc::resource_unavailable_try_again)
            ec.clear();
    }
    TRACE() << spliced << " bytes";
}

channel::state channel::get_state() const
{
    return current_state;
}
