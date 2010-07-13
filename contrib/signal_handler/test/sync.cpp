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

BOOST_AUTO_TEST_CASE(raise_SIGUSR1) 
{ 
    struct sigaction curact; 
    BOOST_CHECK_EQUAL(sigaction(SIGUSR1, 0, &curact), 0); 

    { 
        boost::asio::posix::signal_handler sh(io_service); 
        sh.add_signal(SIGUSR1); 

        BOOST_CHECK_EQUAL(raise(SIGUSR1), 0); 

        int signal = sh.wait(); 
        BOOST_CHECK_EQUAL(signal, SIGUSR1); 
    } 

    struct sigaction curact2; 
    BOOST_CHECK_EQUAL(sigaction(SIGUSR1, 0, &curact2), 0); 
    BOOST_CHECK(std::memcmp(&curact, &curact2, sizeof(struct sigaction))); 
} 

BOOST_AUTO_TEST_CASE(signal_handler_destruction) 
{ 
    struct sigaction curact; 
    BOOST_CHECK_EQUAL(sigaction(SIGUSR1, 0, &curact), 0); 

    { 
        boost::asio::posix::signal_handler sh(io_service); 
        sh.add_signal(SIGUSR1); 

        BOOST_CHECK_EQUAL(raise(SIGUSR1), 0); 
    } 

    struct sigaction curact2; 
    BOOST_CHECK_EQUAL(sigaction(SIGUSR1, 0, &curact2), 0); 
    BOOST_CHECK(std::memcmp(&curact, &curact2, sizeof(struct sigaction))); 
} 
