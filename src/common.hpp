/*
 * common.hpp
 *
 *  Created on: May 25, 2010
 *      Author: nbryskin
 */

#ifndef COMMON_HPP_
#define COMMON_HPP_

#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/channel_logger.hpp>

#ifdef NOTRACE
#define TRACE() while (false) BOOST_LOG(log)
#define TRACE_ERROR(ec) while (false) BOOST_LOG(log)
#else
#define TRACE() BOOST_LOG(log) << __func__ << " "
#define TRACE_ERROR(ec) BOOST_LOG(log) << system_error(ec, __func__).what()
#endif

using namespace boost::asio;
using boost::system::system_error;
using boost::system::error_code;
namespace logging = boost::log;
typedef boost::log::sources::channel_logger<> channel_logger;

#endif /* COMMON_HPP_ */
