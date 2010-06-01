/*
 * statistics.cpp
 *
 *  Created on: May 31, 2010
 *      Author: nbryskin
 */

#include <iomanip>
#include <boost/bind.hpp>

#include "statistics.hpp"

logger statistics::log = logger(keywords::channel = "statistics");
statistics* statistics::instance_;

statistics& statistics::instance()
{
    return *instance_;
}

class statistics::dumper
{
public:
    dumper(const queue* q, std::size_t count)
    : q(q)
    , count(count)
    {
    }

    void dump(std::ostream& stream) const
    {
        q->dump(stream, count);
    }

private:
    const queue* q;
    std::size_t count;
};

std::ostream& operator << (std::ostream& stream, const statistics::dumper& dumper)
{
    dumper.dump(stream);
    return stream;
}

statistics::statistics(asio::io_service& io, const std::string& path, int seconds)
    : dump_timer(io)
    , dump_interval(0, 0, seconds)
    , output(path.c_str())
{
    instance_ = this;
}

void statistics::dump(std::ostream& stream, std::size_t count) const
{
    for (queues_t::const_iterator it = queues.begin(); it != queues.end(); ++it)
    {
        stream << std::fixed << std::setw(10) << std::setprecision(4) << std::setfill('0')
                << it->first << ": " << dumper(it->second, count) << "\t";
    }
    stream << std::endl;
}

void statistics::start()
{
    start_waiting_dump();
}

void statistics::start_waiting_dump()
{
    dump_timer.expires_from_now(dump_interval);
    dump_timer.async_wait(boost::bind(&statistics::finished_waiting_dump, this, placeholders::error()));
}

void statistics::finished_waiting_dump(const error_code& ec)
{
    TRACE_ERROR(ec);
    if (ec)
        return;

    dump(output, 100);
    dump(output);
    output << std::endl;

    start_waiting_dump();
}
