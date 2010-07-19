/*
 * statistics.hpp
 *
 *  Created on: May 31, 2010
 *      Author: nbryskin
 */

#ifndef STATISTICS_HPP_
#define STATISTICS_HPP_

#include <deque>
#include <algorithm>
#include <fstream>
#include <numeric>
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/ptr_container/ptr_set.hpp>
#include <boost/asio.hpp>

#include "stat_sess.hpp"
#include "common.hpp"

class statistics
{
public:
    statistics(asio::io_service& io, const local::stream_protocol::endpoint& stat_ep, std::size_t max_queue_size);

    void start();

    static statistics& instance();

    template<typename value_t>
    static void push(const char* name, value_t value);

    static void increment(const char* name, std::size_t value = 1);
    static void decrement(const char* name, std::size_t value = 1);

    std::string process_request(const std::string& request) const;

    // called by statistics_session (child)
    void finished_session(statistics_session* session, const boost::system::error_code& ec);

protected:
    void start_accept();
    void finished_accept(const error_code& ec, statistics_session* new_session);
    void start_session(statistics_session* new_session);

private:
    std::size_t get_statistic(const std::string& name) const;

    template<typename value_t>
    void push_(const char* name, value_t value);

    void increment_(const char* name, std::size_t value);
    void decrement_(const char* name, std::size_t value);

    class dumper;
    friend std::ostream& operator << (std::ostream& stream, const dumper& dumper);

    class queue
    {
    public:
        virtual ~queue() {}
        virtual void dump(std::ostream& stream, std::size_t count) const = 0;
    };

    template<typename value_type>
    class queue_t : public queue
    {
    public:
        queue_t(std::size_t max_size)
        : max_size(max_size)
        {
        }
        void push(value_type elem)
        {
            if (impl.size() == max_size)
                impl.pop_front();
            impl.push_back(elem);
        }

        virtual void dump(std::ostream& stream, std::size_t count) const
        {
            stream << impl.back() << " " << average(begin(count), end(count)) << " " << *std::max_element(begin(count), end(count));
        }

    protected:
        typedef std::deque<value_type> impl_t;

        typename impl_t::const_iterator begin(std::size_t count) const
        {
            count = count ? std::min(count, impl.size()) : impl.size();
            return impl.begin() + impl.size() - count;
        }

        typename impl_t::const_iterator end(std::size_t count) const
        {
            return impl.end();
        }

        template<typename Iterator>
        typename Iterator::value_type average(Iterator begin, Iterator end) const
        {
            if (begin == end)
                return 0;
            return std::accumulate(begin, end, typename Iterator::value_type()) / (end - begin);
        }
    private:
        impl_t impl;
        std::size_t max_size;
    };

    typedef boost::ptr_map<const char*, queue> queues_t;
    queues_t queues;
    std::size_t max_queue_size;

    typedef std::map<const char*, std::size_t> counters_t;
    counters_t counters;
    local::stream_protocol::acceptor acceptor;

    typedef boost::ptr_set<statistics_session> sessions_t;
    sessions_t sessions;

    static statistics* instance_;
    static logger log;
};

template<typename value_t>
void statistics::push(const char* name, value_t value)
{
    instance().push_(name, value);
}

template<typename value_type>
void statistics::push_(const char* name, value_type value)
{
    typedef queue_t<value_type> queue_type;
    queues_t::iterator it = queues.find(name);
    if (it == queues.end())
        it = queues.insert(name, new queue_type(max_queue_size)).first;
    dynamic_cast<queue_type*>(it->second)->push(value);
}

#endif /* STATISTICS_HPP_ */
