// 
// Copyright (c) 2009, 2010 Boris Schaeling <boris@highscore.de> 
// 
// Distributed under the Boost Software License, Version 1.0. (See accompanying 
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) 
// 

#define BOOST_AUTO_TEST_MAIN 
#include <boost/test/auto_unit_test.hpp> 
#include <cstring> 
#include <signal.h> 
#include "signal_handler.hpp" 

boost::asio::io_service io_service; 

void raise_SIGUSR1_handler(const boost::system::error_code &ec, int signal) 
{ 
    BOOST_CHECK_EQUAL(signal, SIGUSR1); 
} 

BOOST_AUTO_TEST_CASE(raise_SIGUSR1) 
{ 
    struct sigaction curact; 
    BOOST_CHECK_EQUAL(sigaction(SIGUSR1, 0, &curact), 0); 

    { 
        boost::asio::posix::signal_handler sh(io_service); 
        sh.add_signal(SIGUSR1); 

        BOOST_CHECK_EQUAL(raise(SIGUSR1), 0); 

        sh.async_wait(raise_SIGUSR1_handler); 
        io_service.run(); 
        io_service.reset(); 
    } 

    struct sigaction curact2; 
    BOOST_CHECK_EQUAL(sigaction(SIGUSR1, 0, &curact2), 0); 
    BOOST_CHECK(std::memcmp(&curact, &curact2, sizeof(struct sigaction))); 
} 

void aborted_async_call_handler(const boost::system::error_code &ec, int signal) 
{ 
    BOOST_CHECK_EQUAL(ec, boost::asio::error::operation_aborted); 
} 

BOOST_AUTO_TEST_CASE(aborted_async_call) 
{ 
    struct sigaction curact; 
    BOOST_CHECK_EQUAL(sigaction(SIGUSR1, 0, &curact), 0); 

    { 
        boost::asio::posix::signal_handler sh(io_service); 
        sh.add_signal(SIGUSR1); 

        sh.async_wait(aborted_async_call_handler); 
    } 

    io_service.run(); 
    io_service.reset(); 

    struct sigaction curact2; 
    BOOST_CHECK_EQUAL(sigaction(SIGUSR1, 0, &curact2), 0); 
    BOOST_CHECK(std::memcmp(&curact, &curact2, sizeof(struct sigaction))); 
} 

void blocked_async_call_with_local_ioservice_handler(const boost::system::error_code &ec, int signal) 
{ 
    BOOST_CHECK_EQUAL(ec, boost::asio::error::operation_aborted); 
} 

BOOST_AUTO_TEST_CASE(blocked_async_call_with_local_ioservice) 
{ 
    struct sigaction curact; 
    BOOST_CHECK_EQUAL(sigaction(SIGUSR1, 0, &curact), 0); 

    boost::thread t; 

    { 
        boost::asio::io_service io_service; 

        boost::asio::posix::signal_handler sh(io_service); 
        sh.add_signal(SIGUSR1); 

        sh.async_wait(blocked_async_call_with_local_ioservice_handler); 

        // run() is invoked on another thread to make async_wait() call a blocking function. 
        // When sh and io_service go out of scope they should be destroyed properly without 
        // a thread being blocked. 
        t = boost::thread(boost::bind(&boost::asio::io_service::run, boost::ref(io_service))); 
        boost::system_time time = boost::get_system_time(); 
        time += boost::posix_time::time_duration(0, 0, 1); 
        boost::thread::sleep(time); 
    } 

    t.join(); 
    io_service.reset(); 

    struct sigaction curact2; 
    BOOST_CHECK_EQUAL(sigaction(SIGUSR1, 0, &curact2), 0); 
    BOOST_CHECK(std::memcmp(&curact, &curact2, sizeof(struct sigaction))); 
} 

/* 
// Boost.Test always reports an error if a signal is raised 
// which isn't handled intentionally. That's why this test case 
// is commented out (which otherwise works). 

void remove_signal_handler(const boost::system::error_code &ec, int signal) 
{ 
    BOOST_CHECK_EQUAL(ec, boost::asio::error::operation_aborted); 
} 

BOOST_AUTO_TEST_CASE(remove_signal) 
{ 
    struct sigaction curact; 
    BOOST_CHECK_EQUAL(sigaction(SIGUSR1, 0, &curact), 0); 

    boost::thread t; 

    { 
        boost::asio::posix::signal_handler sh(io_service); 
        sh.add_signal(SIGUSR1); 
        sh.remove_signal(SIGUSR1); 

        BOOST_CHECK_EQUAL(raise(SIGUSR1), 0); 

        sh.async_wait(remove_signal_handler); 

        // run() is invoked on another thread to make this test case return. Without using 
        // another thread run() would block as the signal was raised after remove_signal() 
        // had been called. 
        t = boost::thread(boost::bind(&boost::asio::io_service::run, boost::ref(io_service))); 
        boost::system_time time = boost::get_system_time(); 
        time += boost::posix_time::time_duration(0, 0, 1); 
        boost::thread::sleep(time); 
    } 

    t.join(); 
    io_service.reset(); 

    struct sigaction curact2; 
    BOOST_CHECK_EQUAL(sigaction(SIGUSR1, 0, &curact2), 0); 
    BOOST_CHECK(std::memcmp(&curact, &curact2, sizeof(struct sigaction))); 
} 
*/ 

void two_signal_handlers_handler1(const boost::system::error_code &ec, int signal) 
{ 
    BOOST_CHECK_EQUAL(signal, SIGUSR1); 
} 

void two_signal_handlers_handler2(const boost::system::error_code &ec, int signal) 
{ 
    BOOST_CHECK_EQUAL(ec, boost::asio::error::operation_aborted); 
} 

BOOST_AUTO_TEST_CASE(two_signal_handlers) 
{ 
    struct sigaction curact; 
    BOOST_CHECK_EQUAL(sigaction(SIGUSR1, 0, &curact), 0); 

    { 
        boost::asio::posix::signal_handler sh1(io_service); 

        { 
            boost::asio::posix::signal_handler sh2(io_service); 
            sh2.add_signal(SIGUSR1); 
        } 

        BOOST_CHECK_EQUAL(raise(SIGUSR1), 0); 

        sh1.async_wait(two_signal_handlers_handler1); 

        io_service.run(); 
        io_service.reset(); 

        sh1.async_wait(two_signal_handlers_handler2); 
    } 

    io_service.run(); 
    io_service.reset(); 

    struct sigaction curact2; 
    BOOST_CHECK_EQUAL(sigaction(SIGUSR1, 0, &curact2), 0); 
    BOOST_CHECK(std::memcmp(&curact, &curact2, sizeof(struct sigaction))); 
} 
