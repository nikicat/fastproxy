/*
 * common.hpp
 *
 *  Created on: May 25, 2010
 *      Author: nbryskin
 */

#ifndef COMMON_HPP_
#define COMMON_HPP_

// NOTICE: here must not be #include <boost/asio.hpp> because this header is used by
// channel.hpp, where declared asio_handler_* functions, which must be BEFORE asio.hpp

#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <boost/timer.hpp>
#include <boost/date_time.hpp>
#include <string>

#ifdef NOTRACE
#define TRACE() while (false) BOOST_LOG_SEV(log, severity_level::trace)
#define TRACE_ERROR(ec) while (false) BOOST_LOG_SEV(log, severity_level::trace)
#else
#define TRACE() BOOST_LOG_SEV(log, severity_level::trace) << __func__ << " "
#define TRACE_ERROR(ec) BOOST_LOG_SEV(log, severity_level::trace) << system_error(ec, __func__).what() << " "
#endif

using boost::log::trivial::severity_level;
typedef boost::log::sources::severity_channel_logger<severity_level, std::string> logger;

namespace boost { namespace asio { namespace ip {} namespace placeholders {} namespace local {} } }

using boost::system::system_error;
using boost::system::error_code;
using boost::posix_time::time_duration;
namespace keywords = boost::log::keywords;
namespace asio = boost::asio;
namespace ip = asio::ip;
namespace placeholders = asio::placeholders;
namespace local = asio::local;

enum http_error_code
{
    HTTP_BEGIN = 500,
    HTTP_500 = 500,
    HTTP_501,
    HTTP_502,
    HTTP_503,
    HTTP_504,
    HTTP_END,
};


#endif /* COMMON_HPP_ */
