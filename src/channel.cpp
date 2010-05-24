/*
 * channel.cpp
 *
 *  Created on: May 24, 2010
 *      Author: nbryskin
 */

#include <iostream>

#include "channel.hpp"
#include "session.hpp"

const std::size_t PIPE_SIZE = 65536;

channel::channel(ip::tcp::socket& input, ip::tcp::socket& output, session* parent_session) :
    input(input), output(output), waiting_input(false), waiting_output(false), parent_session(parent_session)
{
    if (pipe2(pipe, O_NONBLOCK) == -1)
    {
        perror("pipe2");
        throw boost::system::errc::make_error_code(static_cast<boost::system::errc::errc_t> (errno));
    }
}

void channel::start()
{
    start_waiting_input();
    start_waiting_output();
}

void channel::start_waiting_input()
{
    std::cerr << "start_waiting_input" << std::endl;
    waiting_input = true;
    input.async_read_some(null_buffers(), boost::bind(&channel::finished_waiting_input, this, placeholders::error));
}

void channel::start_waiting_output()
{
    std::cerr << "start_waiting_output" << std::endl;
    waiting_output = true;
    output.async_write_some(null_buffers(), boost::bind(&channel::finished_waiting_output, this, placeholders::error));
}

void channel::finished_waiting_input(const boost::system::error_code& ec)
{
    waiting_input = false;
    std::cerr << "finished_waiting_input ec: " << ec << std::endl;
    if (ec)
        return finish(ec);

    splice_from_input();
}

void channel::finished_waiting_output(const boost::system::error_code& ec)
{
    waiting_output = false;
    std::cerr << "finished_waiting_request_output ec: " << ec << std::endl;
    if (ec)
        return finish(ec);

    splice_to_output();
}

void channel::splice_from_input()
{
    if (input.available() == 0)
    {
        perror("requester connection closed");
        return finish(boost::system::errc::make_error_code(boost::system::errc::not_connected));
    }

    long spliced;
    boost::system::error_code ec;
    splice(input.native(), pipe[1], spliced, ec);
    pipe_size += spliced;
    if (ec)
        finish(ec);

    finished_splice();
}

void channel::splice_to_output()
{
    long spliced;
    boost::system::error_code ec;
    splice(pipe[0], output.native(), spliced, ec);
    pipe_size -= spliced;
    if (ec)
        finish(ec);

    finished_splice();
}

void channel::finished_splice()
{
    assert(!waiting_input || !waiting_output || !"wtf?!?");

    if (pipe_size > 0 && !waiting_input)
        start_waiting_output();

    if (pipe_size < PIPE_SIZE && !waiting_output)
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
        perror("splice");
        ec = boost::system::errc::make_error_code(static_cast<boost::system::errc::errc_t> (errno));
        spliced = 0;

        if (ec == boost::system::errc::resource_unavailable_try_again)
            ec.clear();
    }
    std::cerr << "spliced " << spliced << " bytes" << std::endl;
}
