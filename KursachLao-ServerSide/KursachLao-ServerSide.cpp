#include "ModuleRegistry.h"
#include "RequestHandler.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <iostream>
#include <csignal>


namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// Класс для обработки соединения
class Session : public std::enable_shared_from_this<Session> {
    tcp::socket socket_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    RequestHandler& handler_;  // Ссылка на ваш модуль

public:
    explicit Session(tcp::socket socket, RequestHandler& handler)
        : socket_(std::move(socket)), handler_(handler) {
    }

    void run() {
        do_read();
    }

private:
    void do_read() {
        http::async_read(socket_, buffer_, req_,
            [self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (!ec)
                    self->do_write();
                else
                    std::cerr << "Read error: " << ec.message() << std::endl;
            });
    }

    void do_write() {
        // Создаём лямбду для отправки ответа (аналог `send`)
        auto send = [self = shared_from_this()](http::response<http::string_body> res) {
            http::async_write(self->socket_, std::move(res),
                [self](beast::error_code ec, std::size_t bytes_transferred) {
                    if (!ec) {
                        self->socket_.shutdown(tcp::socket::shutdown_send, ec);
                    }
                    else {
                        std::cerr << "Write error: " << ec.message() << std::endl;
                    }
                });
            };

        // Передаём запрос и `send`-лямбду в ваш модуль
        handler_.handleRequest(std::move(req_), send);
    }
};

std::atomic<bool> running{ true };

// Основной сервер
void run_server(net::io_context& ioc, unsigned short port, RequestHandler& handler) {
    tcp::acceptor acceptor{ ioc, {tcp::v4(), port} };
    while (running.load()) {
        tcp::socket socket{ ioc };

        acceptor.async_accept(socket);
        std::make_shared<Session>(std::move(socket), handler)->run();
    }
}

void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    running.store(false);
}

int main() {
    try {
        // Устанавливаем обработчики сигналов
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        net::io_context ioc;
        unsigned short port = 8080;

        ModuleRegistry registry;

        auto* requestModule = registry.registerModule<RequestHandler>();



        if (!registry.initializeAll()) {
            std::cerr << "Failed to initialize all modules" << std::endl;
            return EXIT_FAILURE;
        }

        std::cout << "All modules initialized successfully" << std::endl;
        std::cout << "Server running on http://localhost:8080" << std::endl;
        std::cout << "Press Ctrl+C to exit" << std::endl;


        // Запускаем сервер с передачей вашего модуля
        run_server(ioc, port, *requestModule);

        // Корректное завершение
        std::cout << "Shutting down modules..." << std::endl;
        registry.shutdownAll();

        std::cout << "Application terminated successfully" << std::endl;
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return 0;
}
