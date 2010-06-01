/*
 * statistics.cpp
 *
 *  Created on: May 31, 2010
 *      Author: nbryskin
 */

#include <iomanip>

#include "statistics.hpp"

statistics& statistics::instance()
{
    static statistics instance_;
    return instance_;
}

statistics::~statistics()
{
    for (queues_t::const_iterator it = queues.begin(); it != queues.end(); ++it)
        delete it->second;
}

class statistics::dumper
{
public:
    dumper(statistics::queue* q, std::size_t count)
    : q(q)
    , count(count)
    {
    }

    void dump(std::ostream& stream) const
    {
        q->dump(stream, count);
    }

private:
    queue* q;
    std::size_t count;
};

std::ostream& operator << (std::ostream& stream, const statistics::dumper& dumper)
{
    dumper.dump(stream);
    return stream;
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
