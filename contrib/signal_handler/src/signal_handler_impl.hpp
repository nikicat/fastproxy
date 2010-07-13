// 
// Copyright (c) 2009, 2010 Boris Schaeling <boris@highscore.de> 
// 
// Distributed under the Boost Software License, Version 1.0. (See accompanying 
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) 
// 

#ifndef BOOST_ASIO_POSIX_SIGNAL_HANDLER_IMPL_HPP 
#define BOOST_ASIO_POSIX_SIGNAL_HANDLER_IMPL_HPP 

#include <boost/asio.hpp> 
#include <boost/thread.hpp> 
#include <boost/bind.hpp> 
#include <boost/system/error_code.hpp> 
#include <boost/system/system_error.hpp> 
#include <map> 
#include <deque> 
#include <signal.h> 
#include <unistd.h> 

namespace boost { 
namespace asio { 
namespace posix { 

class signal_handler_impl 
{ 
public: 
    signal_handler_impl() 
        : sd_(signal_handler_io_service_), 
        run_(true) 
    { 
        int res = pipe(fds_); 
        if (res == -1) 
        { 
            boost::system::system_error e(boost::system::error_code(errno, boost::system::get_system_category()), "boost::asio::posix::signal_handler_impl::signal_handler_impl: pipe failed"); 
            boost::throw_exception(e); 
        } 

        sd_.assign(fds_[0]); 
        begin_read(); 
        signal_handler_thread_ = boost::thread(boost::bind(&boost::asio::io_service::run, &signal_handler_io_service_)); 
    } 

    ~signal_handler_impl() 
    { 
        for (sigactions_t::iterator it = sigactions_.begin(); it != sigactions_.end(); ++it) 
            sigaction(it->first, &it->second, 0); 

        close(fds_[1]); 
        sd_.close(); 

        signal_handler_io_service_.stop(); 
        signal_handler_thread_.join(); 
    } 

    int write_end() const 
    { 
      return fds_[1]; 
    } 

    void add_signal(int signal, struct sigaction oldact) 
    { 
        sigactions_.insert(sigactions_t::value_type(signal, oldact)); 
    } 

    void remove_signal(int signal) 
    { 
        sigactions_t::iterator it = sigactions_.find(signal); 
        if (it != sigactions_.end()) 
        { 
            sigaction(signal, &it->second, 0); 
            sigactions_.erase(it); 
        } 
    } 

    void destroy() 
    { 
        boost::unique_lock<boost::mutex> lock(pending_signals_mutex_); 
        run_ = false; 
        pending_signals_cond_.notify_all(); 
    } 

    int wait(boost::system::error_code &ec) 
    { 
        boost::unique_lock<boost::mutex> lock(pending_signals_mutex_); 
        while (run_ && pending_signals_.empty()) 
            pending_signals_cond_.wait(lock); 
        int signal = 0; 
        if (!pending_signals_.empty()) 
        { 
            signal = pending_signals_.front(); 
            pending_signals_.pop_front(); 
        } 
        else 
            ec = boost::asio::error::operation_aborted; 
        return signal; 
    } 

private: 
    void begin_read() 
    { 
      boost::asio::async_read(sd_, boost::asio::buffer(signal_buffer_), 
          boost::bind(&signal_handler_impl::end_read, this, 
          boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)); 
    } 

    void end_read(const boost::system::error_code &ec, std::size_t bytes_transferred) 
    { 
      if (!ec) 
      { 
        boost::unique_lock<boost::mutex> lock(pending_signals_mutex_); 
        pending_signals_.push_back(signal_buffer_[0]); 
        pending_signals_cond_.notify_all(); 
        begin_read(); 
      } 
    } 

    int fds_[2]; 
    boost::asio::io_service signal_handler_io_service_; 
    boost::thread signal_handler_thread_; 
    boost::asio::posix::stream_descriptor sd_; 
    int signal_buffer_[1]; 
    typedef std::map<int, struct sigaction> sigactions_t; 
    sigactions_t sigactions_; 
    boost::mutex pending_signals_mutex_; 
    boost::condition_variable_any pending_signals_cond_; 
    bool run_; 
    std::deque<int> pending_signals_; 
}; 

} 
} 
} 

#endif 
