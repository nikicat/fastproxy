/*
 * statistics.hpp
 *
 *  Created on: May 31, 2010
 *      Author: nbryskin
 */

#ifndef STATISTICS_HPP_
#define STATISTICS_HPP_

#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
#include <numeric>

class statistics
{
public:
    static statistics& instance();

    template<typename value_t>
    static void push(const char* name, value_t value);

    template<typename value_t>
    void push_(const char* name, value_t value);

    void dump(std::ostream& stream, std::size_t count = 0) const;

private:
    ~statistics();

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
        void push(value_type elem)
        {
            impl.push_back(elem);
        }

        virtual void dump(std::ostream& stream, std::size_t count) const
        {
            stream << average(begin(count), end(count)) << " " << *std::max_element(begin(count), end(count));
        }

    protected:
        typedef std::vector<value_type> impl_t;

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
    };
    typedef std::map<const char*, queue*> queues_t;
    queues_t queues;
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
        it = queues.insert(std::make_pair(name, new queue_type)).first;
    dynamic_cast<queue_type*>(it->second)->push(value);
}

#endif /* STATISTICS_HPP_ */
