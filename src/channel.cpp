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

channel::channel(ip::tcp::socket& input, ip::tcp::socket& output, session& parent_session, const time_duration& input_timeout, bool first_input_stat)
    : input(input)
    , output(output)
    , input_timer(input.io_service())
    , input_timeout(input_timeout)
    , pipe_size(0)
    , parent_session(parent_session)
    , input_handler(boost::bind(&channel::finished_waiting_input, this, placeholders::error(), placeholders::bytes_transferred()))
    , output_handler(boost::bind(&channel::finished_waiting_output,this, placeholders::error(), placeholders::bytes_transferred()))
    , current_state(created)
    , first_input(first_input_stat)
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
    input_timer.expires_from_now(input_timeout);
    input_timer.async_wait(boost::bind(&channel::input_timeouted, this, placeholders::error()));
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
    input_timer.cancel();

    if (ec)
        return finish(ec);

    if (first_input)
    {
        first_input = false;
        statistics::increment("first_received_time", parent_session.timer.elapsed());
    }
    splice_from_input();
}

void channel::finished_waiting_output(const error_code& ec, std::size_t)
{
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);

    splice_to_output();
}

void channel::input_timeouted(const error_code& ec)
{
    TRACE_ERROR(ec);
    if (ec != asio::error::operation_aborted)
    {
        error_code tmp_ec;
        input.cancel(tmp_ec);
    }
}

void channel::splice_from_input()
{
    error_code ec(0, boost::system::generic_category);
    std::size_t avail = input.available(ec);
    if (ec)
        return finish(ec);

    if (avail == 0)
    {
        TRACE() << "connection closed";
        return finish(asio::error::make_error_code(asio::error::eof));
    }
    current_state = splicing_input;

    long spliced;
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
        return finish(asio::error::make_error_code(asio::error::not_socket));
    }

    current_state = splicing_output;
    long spliced;
    error_code ec(0, asio::error::system_category);
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
    TRACE_ERROR(ec) << parent_session.get_id();
    if (ec && ec != asio::error::operation_aborted)
        BOOST_LOG_SEV(log, severity_level::error) << system_error(ec).what();
    current_state = finished;
    parent_session.finished_channel(ec);
}

void channel::splice(int from, int to, long& spliced, error_code& ec)
{
    statistics::increment("total_splices");
    spliced = ::splice(from, 0, to, 0, PIPE_SIZE, SPLICE_F_NONBLOCK | SPLICE_F_MORE | MSG_NOSIGNAL);
    if (spliced == -1)
    {
        ec = asio::error::make_error_code(static_cast<asio::error::basic_errors>(errno));
        TRACE_ERROR(ec);
        spliced = 0;

        if (ec == asio::error::try_again)
            ec.clear();
    }
    statistics::increment("total_bytes", spliced);
    TRACE() << spliced << " bytes";
}

channel::state channel::get_state() const
{
    return current_state;
}
