/*
 * chater.hpp
 *
 *  Created on: Jun 2, 2010
 *      Author: nbryskin
 */

#ifndef CHATER_HPP_
#define CHATER_HPP_

#include <boost/asio.hpp>
#include <iostream>

#include "fastproxy.hpp"
#include "common.hpp"

class chater
{
public:
    chater(asio::io_service& io, int fd)
    : stdin(io, fd)
    , log(keywords::channel = "chater")
    {
    }
    void start()
    {
        start_waiting_input();
        TRACE() << "started";
    }

private:
    void start_waiting_input()
    {
        asio::async_read_until(stdin, input_buffer, '\n', boost::bind(&chater::finished_waiting_input, this, placeholders::error()));
    }

    void finished_waiting_input(const error_code& ec)
    {
        TRACE_ERROR(ec);
        if (ec)
            return;

        std::string command;
        std::istream input(&input_buffer);
        input >> command;
        input.get();

        if (command == "dump")
            dump_action();

        start_waiting_input();
    }

    void dump_action()
    {
        proxy* p = fastproxy::instance().find_proxy();

        p->dump_channels_state();
    }

    asio::posix::stream_descriptor stdin;
    asio::streambuf input_buffer;
    logger log;
};

#endif /* CHATER_HPP_ */
