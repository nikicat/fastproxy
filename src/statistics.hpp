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

class statistics
{
public:
    static statistics& instance();

    static void push(const char* name, double elapsed);

    void push_(const char* name, double elapsed);

    void dump(std::ostream& stream, std::size_t count = 0) const;

private:
    typedef std::vector<double> times_t;
    typedef std::map<const char*, times_t> queues_t;
    queues_t queues;
};

#endif /* STATISTICS_HPP_ */
