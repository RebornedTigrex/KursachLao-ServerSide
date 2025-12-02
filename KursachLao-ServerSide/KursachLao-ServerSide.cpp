#include "LambdaSenders.h"
#include "RequestHandler.h"
#include "ModuleRegistry.h"

#include "macros.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/thread.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include <memory>

namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

void CreateNewHandlers(RequestHandler* module) {

    module->addRouteHandler("/test", [](const sRequest& req, sResponce& res) {
        res.set(http::field::content_type, "text/plain");
        res.body() = "RequestHandler Module Scaling Test. \nТак же проверка поддержки русского языка.";
        }
    );

    module->addRouteHandler("/hello-world-html", [](const sRequest& req, fResponce& res) {
        res.set(http::field::content_type, "text/html");
        res.body() = ;
        }
    );

}

int main()
{
    ModuleRegistry registry;

    auto* requestModule = registry.registerModule<RequestHandler>();

    registry.initializeAll();

    CreateNewHandlers(requestModule);

    try
    {
        bool close = false;
        beast::error_code ec;
        beast::flat_buffer buffer;

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

            LambdaSenders::send_lambda<tcp::socket> lambda{ *socket.get(), close, ec };

            acceptor.accept(*socket);

            http::request<http::string_body> req;
            http::read(*socket.get(), buffer, req, ec);
            if (ec == http::error::end_of_stream) continue;
            if (ec) break;


            requestModule->handleRequest(std::move(req), lambda);
            if (ec) break;
            if (close) continue;

            // Создаем поток с помощью Boost.Thread, используя shared_ptr
            //thread_group.create_thread([socket, buffer, ec, requestModule]() {
            //    try {
            //        http::request<http::string_body> req;
            //        http::read(*socket.get(), buffer, req, ec);
            //        if (ec == http::error::end_of_stream) return;
            //        if (ec) return;

            //        // Отправляем ответ "Hello World!"
            //        requestModule->handle_request(std::move(req), lambda);
            //        if (ec) return;
            //        if (close) break;
            //    }
            //    catch (const std::exception& e) {
            //        std::cerr << "Thread error: " << e.what() << std::endl;
            //    }
            //    });
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}