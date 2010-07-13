// 
// Copyright (c) 2009, 2010 Boris Schaeling <boris@highscore.de> 
// 
// Distributed under the Boost Software License, Version 1.0. (See accompanying 
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) 
// 

#ifndef BOOST_ASIO_POSIX_SIGNAL_HANDLER_HPP 
#define BOOST_ASIO_POSIX_SIGNAL_HANDLER_HPP 

#include "basic_signal_handler.hpp" 
#include "basic_signal_handler_service.hpp" 

namespace boost { 
namespace asio { 
namespace posix { 

typedef basic_signal_handler<basic_signal_handler_service<> > signal_handler; 

} 
} 
} 

#endif 
