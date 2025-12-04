#include "LambdaSenders.h"
#include "RequestHandler.h"
#include "ModuleRegistry.h"
#include "FileCache.h"

#include "macros.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/thread.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include <memory>

#include <fstream>
#include <sstream>

namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace fs = std::filesystem;

void CreateNewHandlers(RequestHandler* module, std::string staticFolder) {

	//fs::path cwd = fs::current_path(); 
    //std::cout << "Current working directory: " << cwd << std::endl;

	

    module->addRouteHandler("/test", [](const sRequest& req, sResponce& res) {
		if (req.method() != http::verb::get) {
            res.result(http::status::method_not_allowed);
            res.set(http::field::content_type, "text/plain");
            res.body() = "Method Not Allowed. Use GET.";
            return;
        }
        res.set(http::field::content_type, "text/plain");
        res.body() = "RequestHandler Module Scaling Test. \nТак же проверка поддержки русского языка.";
        res.result(http::status::ok);
        }
    );


    module->addRouteHandler("/index", [](const sRequest& req, sResponce& res){

        std::string&& file_path = "..\\static\\index.html";
        std::ifstream file(file_path);

        res.set(http::field::content_type, "text/html");
        res.set(http::field::server, "Server");

        res.result(http::status::ok);

        std::stringstream buffer;
        buffer << file.rdbuf();
        res.body() = buffer.str();

        res.keep_alive(req.keep_alive());

        }
    );

}

int main()
{
    ModuleRegistry registry;
    FileCache cache("..\\static");


    auto* requestModule = registry.registerModule<RequestHandler>();

    CreateNewHandlers(requestModule, "..\\static");

    registry.initializeAll();



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