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
#include <boost/asio.hpp>

#include "common.hpp"

class statistics
{
public:
    statistics(asio::io_service& io, time_duration seconds, std::size_t max_queue_size);

    void start();

    static statistics& instance();

    template<typename value_t>
    static void push(const char* name, value_t value);

    template<typename value_t>
    void push_(const char* name, value_t value);

    void dump(std::size_t count = 0) const;

private:
    void start_waiting_dump();
    void finished_waiting_dump(const error_code& ec);

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
    asio::deadline_timer dump_timer;
    time_duration dump_interval;
    std::size_t max_queue_size;

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
