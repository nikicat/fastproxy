// 
// Copyright (c) 2009, 2010 Boris Schaeling <boris@highscore.de> 
// 
// Distributed under the Boost Software License, Version 1.0. (See accompanying 
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) 
// 

#ifndef BOOST_ASIO_POSIX_BASIC_SIGNAL_HANDLER_SERVICE_HPP 
#define BOOST_ASIO_POSIX_BASIC_SIGNAL_HANDLER_SERVICE_HPP 

#include "signal_handler_impl.hpp" 
#include <boost/asio.hpp> 
#include <boost/thread.hpp> 
#include <boost/bind.hpp> 
#include <boost/shared_ptr.hpp> 
#include <boost/weak_ptr.hpp> 
#include <boost/scoped_ptr.hpp> 
#include <boost/system/error_code.hpp> 
#include <boost/system/system_error.hpp> 
#include <cstring> 
#include <signal.h> 
#include <unistd.h> 

namespace boost { 
namespace asio { 
namespace posix { 

template <typename SignalHandlerImplementation = signal_handler_impl> 
class basic_signal_handler_service 
    : public boost::asio::io_service::service 
{ 
public: 
    static boost::asio::io_service::id id; 

    explicit basic_signal_handler_service(boost::asio::io_service &io_service) 
        : boost::asio::io_service::service(io_service), 
        async_wait_work_(new boost::asio::io_service::work(async_wait_io_service_)), 
        async_wait_thread_(boost::bind(&boost::asio::io_service::run, &async_wait_io_service_)) 
    { 
    } 

    ~basic_signal_handler_service() 
    { 
        // The async_wait thread will finish when async_wait_work_ is reset as all asynchronous 
        // operations have been aborted and were discarded before (in destroy). 
        async_wait_work_.reset(); 

        // Event processing is stopped to discard queued operations. 
        async_wait_io_service_.stop(); 

        // The async_wait thread is joined to make sure the signal handler service is 
        // destroyed _after_ the thread is finished (not that the thread tries to access 
        // instance properties which don't exist anymore). 
        async_wait_thread_.join(); 
    } 

    typedef boost::shared_ptr<SignalHandlerImplementation> implementation_type; 

    void construct(implementation_type &impl) 
    { 
        // We must count the I/O objects and can't rely on the shared pointer impl_ 
        // as an asynchronous call can hold a shared pointer at any time, too, thus 
        // preventing the implementation to be destroyed in destroy(). 
        ++count_; 

        implementation_type shared_impl = impl_.lock(); 
        if (shared_impl) 
        { 
            impl = shared_impl; 
        } 
        else 
        { 
            impl.reset(new SignalHandlerImplementation()); 
            impl_ = impl; 
            fd_ = impl->write_end(); 
        } 
    } 

    void destroy(implementation_type &impl) 
    { 
        // If an asynchronous call is currently waiting for a signal 
        // we must interrupt the blocked call to make sure it returns. 
        if (!--count_) 
            impl->destroy(); 

        impl.reset(); 
    } 

    void add_signal(implementation_type &impl, int signal, int flags, sigset_t *mask) 
    { 
        struct sigaction act, oldact; 

        std::memset(&act, 0, sizeof(struct sigaction)); 
        act.sa_handler = basic_signal_handler_service<SignalHandlerImplementation>::signal_handler; 
        act.sa_flags = flags; 
        if (mask) 
            act.sa_mask = *mask; 

        int res = sigaction(signal, &act, &oldact); 
        if (res == -1) 
        { 
            boost::system::system_error e(boost::system::error_code(errno, boost::system::get_system_category()), "boost::asio::posix::basic_signal_handler_service::add_signal: sigaction failed"); 
            boost::throw_exception(e); 
        } 

        impl->add_signal(signal, oldact); 
    } 

    void remove_signal(implementation_type &impl, int signal) 
    { 
        impl->remove_signal(signal); 
    } 

    int wait(implementation_type &impl, boost::system::error_code &ec) 
    { 
        return impl->wait(ec); 
    } 

    template <typename Handler> 
    class wait_operation 
    { 
    public: 
        wait_operation(implementation_type &impl, boost::asio::io_service &io_service, Handler handler) 
            : impl_(impl), 
            io_service_(io_service), 
            work_(io_service), 
            handler_(handler) 
        { 
        } 

        void operator()() const 
        { 
            implementation_type impl = impl_.lock(); 
            if (impl) 
            { 
                boost::system::error_code ec; 
                int signal = impl->wait(ec); 
                this->io_service_.post(boost::asio::detail::bind_handler(handler_, ec, signal)); 
            } 
            else 
            { 
                this->io_service_.post(boost::asio::detail::bind_handler(handler_, boost::asio::error::operation_aborted, 0)); 
            } 
        } 

    private: 
        boost::weak_ptr<SignalHandlerImplementation> impl_; 
        boost::asio::io_service &io_service_; 
        boost::asio::io_service::work work_; 
        Handler handler_; 
    }; 

    template <typename Handler> 
    void async_wait(implementation_type &impl, Handler handler) 
    { 
        this->async_wait_io_service_.post(wait_operation<Handler>(impl, this->get_io_service(), handler)); 
    } 

private: 
    void shutdown_service() 
    { 
    } 

    static void signal_handler(int signal) 
    { 
        while (::write(fd_, &signal, sizeof(signal)) == -1 && errno == EINTR); 
    } 

    static int count_; 
    static boost::weak_ptr<SignalHandlerImplementation> impl_; 
    static int fd_; 
    boost::asio::io_service async_wait_io_service_; 
    boost::scoped_ptr<boost::asio::io_service::work> async_wait_work_; 
    boost::thread async_wait_thread_; 
}; 

template <typename SignalHandlerImplementation> 
boost::asio::io_service::id basic_signal_handler_service<SignalHandlerImplementation>::id; 

template <typename SignalHandlerImplementation> 
int basic_signal_handler_service<SignalHandlerImplementation>::count_ = 0; 

template <typename SignalHandlerImplementation> 
boost::weak_ptr<SignalHandlerImplementation> basic_signal_handler_service<SignalHandlerImplementation>::impl_; 

template <typename SignalHandlerImplementation> 
int basic_signal_handler_service<SignalHandlerImplementation>::fd_; 

} 
} 
} 

#endif 
