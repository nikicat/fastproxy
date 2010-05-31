/*
 * statistics.hpp
 *
 *  Created on: May 31, 2010
 *      Author: nbryskin
 */

#ifndef STATISTICS_HPP_
#define STATISTICS_HPP_

#include <numeric>

class statistics
{
public:
    static statistics& instance()
    {
        static statistics instance_;
        return instance_;
    }

    static void push(const char* name, double elapsed)
    {
        instance().push_(name, elapsed);
    }

    void push_(const char* name, double elapsed)
    {
        queues[name].push_back(elapsed);
    }

    void dump(std::ostream& stream, std::size_t count = 0) const
    {
        for(queues_t::const_iterator times = queues.begin(); times != queues.end(); ++times)
        {
            stream << times->first << "=" << average(times->first, count) << "\t";
        }
        stream << std::endl;
    }

    double average(const char* queue_name, std::size_t count = 0) const
    {
        const times_t& times = queues.find(queue_name)->second;
        count = count ? count : times.size();
        return std::accumulate(times.begin() + times.size() - count, times.end(), 0.0) / count;
    }

private:
    typedef std::vector<double> times_t;
    typedef std::map<const char*, times_t> queues_t;
    queues_t queues;
};

#endif /* STATISTICS_HPP_ */
