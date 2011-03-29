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
#include <boost/filesystem.hpp>

#include "statistics.hpp"

logger statistics::log = logger(keywords::channel = "statistics");
statistics* statistics::instance_;

statistics& statistics::instance()
{
    return *instance_;
}

bool operator < (const statistics_session& lhs, const statistics_session& rhs)
{
    return &lhs < &rhs;
}

statistics::statistics(asio::io_service& io, const local::stream_protocol::endpoint& stat_ep)
    : acceptor(io, stat_ep, true)
{
    instance_ = this;
}

statistics::~statistics()
{
    try
    {
        boost::filesystem::remove(acceptor.local_endpoint().path());
    }
    catch (const std::exception& e)
    {
        BOOST_LOG_SEV(log, severity_level::warning) << e.what();
    }
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
    if (request == "show stat")
    {
        for (counters_t::const_iterator it = counters.begin(); it != counters.end(); ++it)
            response << it->first << "\t";
        response.seekp(-1, std::ios_base::cur);
        response << "\n";
        for (counters_t::const_iterator it = counters.begin(); it != counters.end(); ++it)
            response << it->second << "\t";
        response.seekp(-1, std::ios_base::cur);
        response << "\n";
    }
    else
    {
        boost::split(tokens, request, boost::is_any_of(" \t,"));
        for (split_vector_type::const_iterator it = tokens.begin(); it != tokens.end(); ++it)
        {
            try
            {
                response << get_statistic(*it);
            }
            catch (const boost::bad_index& e)
            {
                response << e.what() << "?";
            }
            catch (const std::invalid_argument& e)
            {
                response << e.what() << "?";
            }
            catch (const boost::bad_lexical_cast& e)
            {
                response << "need_integer";
            }
            response << (it + 1 == tokens.end() ? '\n' : '\t');
        }
    }

    return response.str();
}

statistics::value_t statistics::get_statistic(const std::string& name) const
{
    for (counters_t::const_iterator it = counters.begin(); it != counters.end(); ++it)
        if (name.compare(it->first) == 0)
            return it->second;

    throw boost::bad_index(name.c_str());
}

void statistics::increment(const char* name, long value)
{
    instance().add(name, value);
}

void statistics::decrement(const char* name, long value)
{
    instance().add(name, -value);
}

void statistics::increment(const char* name, double value)
{
    instance().add(name, value);
}

template<typename T>
void statistics::add(const char* name, T value)
{
    auto counter = counters.find(name);
    if (counter == counters.end())
        counters[name] = value;
    else
        boost::get<T>(counter->second) += value;
}
