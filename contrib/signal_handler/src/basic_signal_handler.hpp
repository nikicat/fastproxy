// 
// Copyright (c) 2009, 2010 Boris Schaeling <boris@highscore.de> 
// 
// Distributed under the Boost Software License, Version 1.0. (See accompanying 
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) 
// 

#ifndef BOOST_ASIO_POSIX_BASIC_SIGNAL_HANDLER_HPP 
#define BOOST_ASIO_POSIX_BASIC_SIGNAL_HANDLER_HPP 

#include <boost/asio.hpp> 
#include <signal.h> 

namespace boost { 
namespace asio { 
namespace posix { 

template <typename Service> 
class basic_signal_handler 
    : public boost::asio::basic_io_object<Service> 
{ 
public: 
    explicit basic_signal_handler(boost::asio::io_service &io_service) 
        : boost::asio::basic_io_object<Service>(io_service) 
    { 
    } 

    void add_signal(int signal, int flags = 0, sigset_t *mask = 0) 
    { 
        this->service.add_signal(this->implementation, signal, flags, mask); 
    } 

    void remove_signal(int signal) 
    { 
        this->service.remove_signal(this->implementation, signal); 
    } 

    int wait() 
    { 
        boost::system::error_code ec; 
        int signal = this->service.wait(this->implementation, ec); 
        boost::asio::detail::throw_error(ec); 
        return signal; 
    } 

    int wait(boost::system::error_code &ec) 
    { 
        return this->service.wait(this->implementation, ec); 
    } 

    template <typename Handler> 
    void async_wait(Handler handler) 
    { 
        this->service.async_wait(this->implementation, handler); 
    } 
}; 

} 
} 
} 

#endif 
