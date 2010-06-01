/*
 * common.hpp
 *
 *  Created on: May 25, 2010
 *      Author: nbryskin
 */

#ifndef COMMON_HPP_
#define COMMON_HPP_

#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <boost/timer.hpp>
#include <string>

#ifdef NOTRACE
#define TRACE() while (false) BOOST_LOG_SEV(log, severity_level::trace)
#define TRACE_ERROR(ec) while (false) BOOST_LOG_SEV(log, severity_level::trace)
#else
#define TRACE() BOOST_LOG_SEV(log, severity_level::trace) << __func__ << " "
#define TRACE_ERROR(ec) BOOST_LOG_SEV(log, severity_level::trace) << system_error(ec, __func__).what()
#endif

using boost::log::trivial::severity_level;
typedef boost::log::sources::severity_channel_logger<severity_level, std::string> logger;

namespace boost { namespace asio { namespace ip {} namespace placeholders {} } }

//using namespace boost::asio;
using boost::system::system_error;
using boost::system::error_code;
namespace keywords = boost::log::keywords;
namespace asio = boost::asio;
namespace ip = asio::ip;
namespace placeholders = asio::placeholders;

#endif /* COMMON_HPP_ */
