#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

#include <iostream>
#include <algorithm>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>
#include <boost/system/linux_error.hpp>

using namespace boost::asio;

typedef posix::stream_descriptor bsd;

const std::size_t PIPE_SIZE = 65536;

typedef boost::function<void (const boost::system::error_code&)> CompletionHandler;

class session : public boost::noncopyable
{
public:
    session(io_service& io) :
        requester(io), responser(io), resolver(io)
    {
        create_pipe(request_pipe);
        create_pipe(response_pipe);
    }

    void create_pipe(int(&pipe)[2])
    {
        if (pipe2(pipe, O_NONBLOCK) == -1)
        {
            perror("pipe2");
            throw boost::system::errc::make_error_code(static_cast<boost::system::errc::errc_t>(errno));
        }
    }

    ip::tcp::socket& socket()
    {
        return requester;
    }

    void start(const CompletionHandler& completion)
    {
        this->completion = completion;
        start_receive_header();
    }

    void finish(const boost::system::error_code& ec)
    {
        CompletionHandler c(completion);
        completion = boost::bind(&session::print_error_code, this, placeholders::error);
        c(ec);
    }

    void print_error_code(const boost::system::error_code& ec)
    {
        std::cerr << ec << std::endl;
    }

    void start_receive_header()
    {
        requester.async_receive(buffer(header), requester.message_peek, boost::bind(&session::finished_receive_header, this, placeholders::error,
                placeholders::bytes_transferred));
    }

    void finished_receive_header(const boost::system::error_code& ec, std::size_t bytes_transferred)
    {
        std::cerr << "handle_header" << std::endl;
        if (ec)
            return finish(ec);
        std::string peer = parse_header(bytes_transferred);
        start_resolving(peer);
    }

    void start_resolving(const std::string& peer)
    {
        resolver.async_resolve(ip::tcp::resolver::query(peer, default_http_port), boost::bind(&session::finished_resolving, this,
                placeholders::error, placeholders::iterator));
    }

    void finished_resolving(const boost::system::error_code& ec, ip::tcp::resolver::iterator iterator)
    {
        if (ec)
            return finish(ec);
        start_connecting_to_peer(iterator);
    }

    void start_connecting_to_peer(ip::tcp::resolver::iterator iterator)
    {
        std::cerr << "connecting to " << iterator->endpoint() << std::endl;
        responser.async_connect(iterator->endpoint(), boost::bind(&session::finished_connecting_to_peer, this, placeholders::error));
    }

    void finished_connecting_to_peer(const boost::system::error_code& ec)
    {
        if (ec)
            return finish(ec);
        start_waiting_request_input();
        start_waiting_response_input();
    }

    void start_waiting_request_input()
    {
        std::cerr << "start_waiting_request_input" << std::endl;
        requester.async_read_some(null_buffers(), boost::bind(&session::finished_waiting_request_input, this, placeholders::error));
    }

    void start_waiting_request_output()
    {
        std::cerr << "start_waiting_request_output" << std::endl;
        responser.async_write_some(null_buffers(), boost::bind(&session::finished_waiting_request_output, this, placeholders::error));
    }

    void finished_waiting_request_input(const boost::system::error_code& ec)
    {
        std::cerr << "finished_waiting_request_input ec: " << ec << std::endl;
        if (ec)
            return finish(ec);

        splice_request();
    }

    void splice_request()
    {
        if (requester.available() == 0)
        {
            perror("requester connection closed");
            return completion(boost::system::errc::make_error_code(boost::system::errc::not_connected));
        }

        boost::system::error_code splice_ec;
        splice(requester.native(), pipe[1], splice_ec);

        if (splice_ec)
        {
            if (splice_ec == boost::system::errc::resource_unavailable_try_again)
                start_waiting_request_output();
            else
                return finish(splice_ec);
        }
        else
        {
            start_waiting_request_input();
        }
    }

    void finished_waiting_request_output(const boost::system::error_code& ec)
    {
        std::cerr << "finished_waiting_request_output ec: " << ec << std::endl;
        if (ec)
            return finish(ec);

        boost::system::error_code splice_ec;
        splice(pipe[0], responser.native(), splice_ec);

        if (splice_ec)
        {
            if (splice_ec == boost::system::errc::resource_unavailable_try_again)
                start_waiting_request_output();
            else
                return finish(splice_ec);
        }
        else
        {
            start_waiting_request_input();
        }
    }

    void finished_waiting_request()
    {
        boost::system::error_code ec;
        wait_for_t wait_for;
        double_splice(requester, responser, request_pipe, wait_for, ec);
        if (ec)
            return finish(ec);

        switch (wait_for)
        {
            case wait_input:
                start_waiting_request_input();
                break;
            case wait_output:
                start_waiting_request_output();
                break;
        }
    }

    void start_waiting_response_input()
    {
        std::cerr << "start_waiting_response_input" << std::endl;
        responser.async_read_some(null_buffers(), boost::bind(&session::finished_waiting_response_input, this, placeholders::error));
    }

    void start_waiting_response_output()
    {
        std::cerr << "start_waiting_response_output" << std::endl;
        requester.async_write_some(null_buffers(), boost::bind(&session::finished_waiting_response_output, this, placeholders::error));
    }

    void finished_waiting_response_input(const boost::system::error_code& ec)
    {
        std::cerr << "finished_waiting_response_input ec: " << ec << std::endl;
        if (ec)
            return finish(ec);

        if (responser.available() == 0)
        {
            perror("responser connection closed");
            return completion(boost::system::errc::make_error_code(boost::system::errc::not_connected));
        }

        finished_waiting_response();
    }

    void finished_waiting_response_output(const boost::system::error_code& ec)
    {
        std::cerr << "finished_waiting_response_output ec: " << ec << std::endl;
        if (ec)
            return finish(ec);

        finished_waiting_response();
    }

    void finished_waiting_response()
    {
        boost::system::error_code ec;
        wait_for_t wait_for;
        splice(responser, requester, response_pipe, wait_for, ec);
        if (ec)
            return finish(ec);

        switch (wait_for)
        {
            case wait_input:
                start_waiting_response_input();
                break;
            case wait_output:
                start_waiting_response_output();
                break;
        }
    }

protected:
    std::string parse_header(std::size_t size)
    {
        char* begin = std::find(header.begin(), header.begin() + size, ' ') + 8;
        char* end = std::find(begin, header.begin() + size, '/');
        return std::string(begin, end);
    }

    enum wait_for_t
    {
        wait_input,
        wait_output,
    };

    void double_splice(ip::tcp::socket& from, ip::tcp::socket& to, int pipe[2], wait_for_t& wait_for, boost::system::error_code& ec)
    {
        wait_for = wait_input;
        splice(from.native(), pipe[1], ec);
        if (ec == boost::system::errc::resource_unavailable_try_again)
            wait_for = wait_output;

        splice(pipe[0], to.native(), ec);
        if (ec == boost::system::errc::resource_unavailable_try_again)
            wait_for = wait_output;
    }

    void splice(int from, int to, boost::system::error_code& ec)
    {
        long spliced = ::splice(from, 0, to, 0, PIPE_SIZE, SPLICE_F_NONBLOCK);
        if (spliced == -1)
        {
            perror("splice");
            ec = boost::system::errc::make_error_code(static_cast<boost::system::errc::errc_t>(errno));
        }
        std::cerr << "spliced " << spliced << " bytes" << std::endl;
    }

private:
    ip::tcp::socket requester;
    int request_pipe[2];
    int response_pipe[2];
    ip::tcp::socket responser;
    CompletionHandler completion;
    const static std::size_t http_header_head_max_size = sizeof('GET http://') + 256;
    boost::array<char, http_header_head_max_size> header;
    ip::tcp::resolver resolver;
    static std::string default_http_port;
};

std::string session::default_http_port = std::string("80");

typedef boost::shared_ptr<session> session_ptr;

class proxy : public boost::noncopyable
{
public:
    proxy(io_service& io, ip::tcp::endpoint inbound) :
        acceptor(io, inbound)
    {
        acceptor.listen();
    }

    void start_accept()
    {
        session_ptr new_sess(new session(acceptor.io_service()));
        acceptor.async_accept(new_sess->socket(), boost::bind(&proxy::handle_accept, this, placeholders::error, new_sess));
    }

    void handle_accept(const boost::system::error_code& ec, session_ptr new_sess)
    {
        if (ec)
        {
            acceptor.io_service().stop();
            return;
        }

        sessions.insert(new_sess);
        new_sess->start(boost::bind(&proxy::session_closed, this, new_sess, placeholders::error));
        start_accept();
    }

    void session_closed(session_ptr session, const boost::system::error_code& ec)
    {
        std::cerr << "session finished. ec=" << ec << std::endl;
        sessions.erase(session);
    }

private:
    ip::tcp::acceptor acceptor;
    std::set<session_ptr> sessions;
};

namespace po = boost::program_options;

int main(int argc, char* argv[])
{
    po::options_description desc("Allowed options");
    desc.add_options()("help", "produce help message")("listen", po::value<boost::uint16_t>(), "listening port");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help") || !vm.count("listen"))
    {
        std::cout << desc << std::endl;
        return 1;
    }

    ip::tcp::endpoint inbound(ip::tcp::v4(), vm["listen"].as<boost::uint16_t> ());

    io_service io;
    proxy p(io, inbound);
    p.start_accept();

    io.run();

    return 0;
}
