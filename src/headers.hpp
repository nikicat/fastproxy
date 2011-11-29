/*
 * headers.hpp
 *
 *  Created on: Jul 8, 2010
 *      Author: nbryskin
 */

#ifndef HEADERS_HPP_
#define HEADERS_HPP_

#include <set>
#include <functional>
#include <cstring>

class lstring
{
public:
    lstring()
      : begin()
      , end()
    {
    }

    lstring(const char* cstr)
      : begin(cstr)
      , end(begin + std::strlen(cstr))
    {
    }

    lstring(const char* begin, const char* end)
      : begin(begin)
      , end(end)
    {
    }

    operator bool () const
    {
        return size() != 0;
    }

    std::size_t size() const
    {
        return end - begin;
    }

    friend bool operator < (const lstring& lhs, const lstring& rhs)
    {
        for (const char *l = lhs.begin, *r = rhs.begin; l != lhs.end && r != rhs.end; l++, r++)
        {
            if (tolower(*l) < tolower(*r))
                return true;

            if (tolower(*l) > tolower(*r))
                break;
        }

        return false;
    }

    const char* begin;
    const char* end;
};

typedef std::set<lstring> headers_type;

#endif /* HEADERS_HPP_ */
