/*
 * statistics.cpp
 *
 *  Created on: May 31, 2010
 *      Author: nbryskin
 */

#include <iomanip>
#include <boost/bind.hpp>
#include <boost/date_time.hpp>
#include <boost/algorithm/string.hpp>

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

bool operator < (const statistics_session& lhs, const statistics_session& rhs)
{
    return &lhs < &rhs;
}

statistics::statistics(asio::io_service& io, const local::stream_protocol::endpoint& stat_ep, std::size_t max_queue_size)
    : max_queue_size(max_queue_size)
    , acceptor(io, stat_ep)
{
    instance_ = this;
}

void statistics::start()
{
    start_accept();
    TRACE() << "started";
}

void statistics::start_accept()
{
    std::unique_ptr<statistics_session> new_sess(new statistics_session(acceptor.io_service(), *this));
    acceptor.async_accept(new_sess->socket(), boost::bind(&statistics::finished_accept, this, placeholders::error(), new_sess.get()));
    new_sess.release();
}

void statistics::finished_accept(const error_code& ec, statistics_session* new_session)
{
    std::unique_ptr<statistics_session> session_ptr(new_session);
    TRACE_ERROR(ec);
    if (ec)
        return;

    start_session(session_ptr.get());
    session_ptr.release();
}

void statistics::start_session(statistics_session* new_session)
{
    bool inserted = sessions.insert(new_session).second;
    assert(inserted);
    new_session->start();
    start_accept();
    statistics::increment("total_stat_sessions");
    statistics::increment("current_stat_sessions");
}

void statistics::finished_session(statistics_session* session, const boost::system::error_code& ec)
{
    TRACE_ERROR(ec);
    std::size_t c = sessions.erase(*session);
    if (c != 1)
        TRACE() << "erased " << c << " items. total " << sessions.size() << " items";
    assert(c == 1);
    statistics::decrement("current_stat_sessions");
}

std::string statistics::process_request(const std::string& request) const
{
    typedef std::vector<std::string> split_vector_type;

    std::ostringstream response;
    split_vector_type tokens;
    boost::split(tokens, request, boost::is_any_of(" \t,"));
    for (split_vector_type::const_iterator it = tokens.begin(); it != tokens.end(); ++it)
    {
        std::size_t value = get_statistic(*it);
        if (value == std::numeric_limits<std::size_t>::max())
            response << "???";
        else
            response << value;
        response << (it + 1 == tokens.end() ? '\n' : '\t');
    }

    return response.str();
}

std::size_t statistics::get_statistic(const std::string& name) const
{
    for (counters_t::const_iterator it = counters.begin(); it != counters.end(); ++it)
        if (name.compare(it->first) == 0)
            return it->second;

    return std::numeric_limits<std::size_t>::max();
}

void statistics::increment(const char* name, std::size_t value)
{
    instance().increment_(name, value);
}

void statistics::decrement(const char* name, std::size_t value)
{
    instance().decrement_(name, value);
}

void statistics::increment_(const char* name, std::size_t value)
{
    counters[name] += value;
}

void statistics::decrement_(const char* name, std::size_t value)
{
    counters[name] -= value;
}
