/*
 * statistics.cpp
 *
 *  Created on: May 31, 2010
 *      Author: nbryskin
 */

#include <numeric>
#include <algorithm>
#include <iostream>
#include <iomanip>

#include "statistics.hpp"

template<typename Iterator>
typename Iterator::value_type average(Iterator begin, Iterator end)
{
    if (begin == end)
        return 0;
    return std::accumulate(begin, end, typename Iterator::value_type()) / (end - begin);
}

statistics& statistics::instance()
{
    static statistics instance_;
    return instance_;
}

void statistics::push(const char* name, double elapsed)
{
    instance().push_(name, elapsed);
}

void statistics::push_(const char* name, double elapsed)
{
    queues[name].push_back(elapsed);
}

void statistics::dump(std::ostream& stream, std::size_t count) const
{
    for(queues_t::const_iterator times = queues.begin(); times != queues.end(); ++times)
    {
        count = count ? std::min(count, times->second.size()) : times->second.size();
        times_t::const_iterator begin = times->second.begin() + times->second.size() - count;
        times_t::const_iterator end = times->second.end();
        stream << std::fixed << std::setw(10) << std::setprecision(4) << std::setfill('0')
                << times->first << ": "
                << average(begin, end) << " "
                << *std::max_element(begin, end) << "\t";
    }
    stream << std::endl;
}
