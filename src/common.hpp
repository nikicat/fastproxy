/*
 * common.hpp
 *
 *  Created on: May 25, 2010
 *      Author: nbryskin
 */

#ifndef COMMON_HPP_
#define COMMON_HPP_

#include <boost/log/sources/record_ostream.hpp>

#define TRACE() BOOST_LOG(log) << __func__
#define TRACE_HANDLER(ec) BOOST_LOG(log) << system_error(ec, __func__).what()

#endif /* COMMON_HPP_ */
