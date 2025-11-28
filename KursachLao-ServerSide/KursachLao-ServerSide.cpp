#include "SessionHandler.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/thread.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include <memory>

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

        boost::thread_group thread_group;

        for (;;)
        {
            // Создаем shared_ptr для socket
            auto socket = boost::make_shared<tcp::socket>(ioc);
            acceptor.accept(*socket);

            // Создаем поток с помощью Boost.Thread, используя shared_ptr
            thread_group.create_thread([socket]() {
                try {
                    SessionHandler::do_session(std::move(*socket));
                }
                catch (const std::exception& e) {
                    std::cerr << "Thread error: " << e.what() << std::endl;
                }
                });
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}