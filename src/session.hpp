/*
 * session.hpp
 *
 *  Created on: May 24, 2010
 *      Author: nbryskin
 */

#ifndef SESSION_HPP_
#define SESSION_HPP_

#include <boost/smart_ptr.hpp>
#include <boost/function.hpp>
#include <boost/log/sources/channel_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include "channel.hpp"
#include "common.hpp"

using namespace boost::asio;
using boost::system::system_error;
using boost::system::error_code;
namespace logging = boost::log;

class proxy;

class session : public boost::noncopyable, public boost::enable_shared_from_this<session>
{
public:
    session(io_service& io, proxy& parent_proxy);

    ip::tcp::socket& socket();

    // called by proxy (parent)
    void start();

    // called by channel (child)
    void finish(const error_code& ec);

protected:
    void start_receive_header();
    void finished_receive_header(const error_code& ec, std::size_t bytes_transferred);

    void start_resolving(const std::pair<std::string, std::string>& peer);
    void finished_resolving(const error_code& ec, ip::tcp::resolver::iterator iterator);

    void start_connecting_to_peer(ip::tcp::resolver::iterator iterator);
    void finished_connecting_to_peer(const error_code& ec);

    std::pair<std::string, std::string> parse_header(std::size_t size);

private:
    proxy& parent_proxy;
    ip::tcp::socket requester;
    ip::tcp::socket responder;
    ip::tcp::resolver resolver;
    channel request_channel;
    channel response_channel;
    const static std::size_t http_header_head_max_size = sizeof("GET http://") + 256;
    boost::array<char, http_header_head_max_size> header;
    static std::string default_http_port;
    int opened_channels;
    logging::sources::channel_logger<> log;
};

typedef boost::shared_ptr<session> session_ptr;

#endif /* SESSION_HPP_ */
