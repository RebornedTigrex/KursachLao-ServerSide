#include "SessionHeandler.h"
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include <thread>

namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

int main()
{
    try
    {
        auto const address = net::ip::make_address("0.0.0.0");
        auto const port = static_cast<unsigned short>(8080);

        net::io_context ioc{ 1 };
        tcp::acceptor acceptor{ ioc, {address, port} };

        std::cout << "Server running on http://0.0.0.0:8080" << std::endl;
        std::cout << "Open http://localhost:8080 in your browser to see 'Hello World!'" << std::endl;

        for (;;)
        {
            tcp::socket socket{ ioc };
            acceptor.accept(socket);
            std::thread{ std::bind(&SessionHandler::do_session, std::move(socket)) }.detach();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}