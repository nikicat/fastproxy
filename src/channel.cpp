/*
 * channel.cpp
 *
 *  Created on: May 24, 2010
 *      Author: nbryskin
 */

#include <iostream>
#include <boost/format.hpp>
#include <boost/log/sources/channel_feature.hpp>

#include "channel.hpp"
#include "session.hpp"

#include "common.hpp"

using namespace boost::log;
using namespace boost::system;

const long PIPE_SIZE = 65536;

channel::channel(ip::tcp::socket& input, ip::tcp::socket& output, session* parent_session)
    : input(input)
    , output(output)
    , pipe_size(0)
    , parent_session(parent_session)
    , log(boost::log::keywords::channel = "channel")
{
    if (pipe2(pipe, O_NONBLOCK) == -1)
    {
        perror("pipe2");
        throw boost::system::errc::make_error_code(static_cast<boost::system::errc::errc_t> (errno));
    }
}

void channel::start()
{
    //sources::channel_logger<> new_log(keywords::channel = (boost::format("channel %1%->%2%") % input.native() % output.native()).str());
    //log.swap(new_log);
    start_waiting();
}

void channel::start_waiting_input()
{
    TRACE();
    input.async_read_some(null_buffers(), boost::bind(&channel::finished_waiting_input, this, placeholders::error));
}

void channel::start_waiting_output()
{
    TRACE();
    output.async_write_some(null_buffers(), boost::bind(&channel::finished_waiting_output, this, placeholders::error));
}

void channel::finished_waiting_input(const boost::system::error_code& ec)
{
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);

    splice_from_input();
}

void channel::finished_waiting_output(const boost::system::error_code& ec)
{
    TRACE_ERROR(ec);
    if (ec)
        return finish(ec);

    splice_to_output();
}

void channel::splice_from_input()
{
    if (input.available() == 0)
    {
        TRACE() << "requester connection closed";
        return finish(boost::system::errc::make_error_code(boost::system::errc::not_connected));
    }

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

void channel::finish(const boost::system::error_code& ec)
{
    parent_session->finish(ec);
}

void channel::splice(int from, int to, long& spliced, boost::system::error_code& ec)
{
    spliced = ::splice(from, 0, to, 0, PIPE_SIZE, SPLICE_F_NONBLOCK);
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
