#include "LambdaSenders.h"
#include "RequestHandler.h"
#include "ModuleRegistry.h"
#include "FileCache.h"
#include "macros.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <memory>
#include <fstream>
#include <sstream>
#include <atomic>

namespace po = boost::program_options;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
namespace fs = std::filesystem;
namespace beast = boost::beast;

void printConnectionInfo(tcp::socket& socket) {
    try {
        tcp::endpoint remote_ep = socket.remote_endpoint();
        boost::asio::ip::address client_address = remote_ep.address();
        unsigned short client_port = remote_ep.port();

        std::cout << "Client connected from: "
            << client_address.to_string()
            << ":" << client_port << std::endl;
    }
    catch (const boost::system::system_error& e) {
        std::cerr << "Error getting connection info: " << e.what() << std::endl;
    }
}

void CreateNewHandlers(RequestHandler* module, std::string staticFolder) {
    // Теперь обработчики должны работать через новый интерфейс
    module->addRouteHandler("/test", [](const sRequest& req, sResponce& res) {
        if (req.method() != http::verb::get) {
            res.result(http::status::method_not_allowed);
            res.set(http::field::content_type, "text/plain");
            res.body() = "Method Not Allowed. Use GET.";
            return;
        }
        res.set(http::field::content_type, "text/plain");
        res.body() = "RequestHandler Module Scaling Test.\nAlso checking support for the Russian language.";
        res.result(http::status::ok);
        });

    // Обработчик для wildcard должен использовать serveStaticFile
    // Если вы хотите использовать новый функционал, добавьте:
    module->addRouteHandler("/api/data", [](const sRequest& req, sResponce& res) {
        // Новый обработчик с query параметрами
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"status": "success", "message": "API endpoint"})";
        res.result(http::status::ok);
        });

    // Wildcard обработчик для статических файлов остается
    module->addRouteHandler("/*", [](const sRequest& req, sResponce& res) {
        // Обработка будет в serveStaticFile
        });
}

// Сессия для обработки соединения
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, RequestHandler* handler, FileCache* cache)
        : socket_(std::move(socket))
        , handler_(handler)
        , cache_(cache)
        , strand_(socket_.get_executor())  // Прямая инициализация strand из executor
    {
    }

    void start() {
        // Запускаем асинхронную обработку в корутине
        net::spawn(strand_,
            [self = shared_from_this()](net::yield_context yield) {
                self->do_session(yield);
            });
    }

private:
    void do_session(net::yield_context yield) {
        beast::error_code ec;
        beast::flat_buffer buffer;

        for (;;) {
            // Асинхронное чтение запроса
            http::request<http::string_body> req;
            http::async_read(socket_, buffer, req, yield[ec]);

            if (ec == http::error::end_of_stream || ec == net::error::eof) {
                // Нормальное закрытие соединения
                break;
            }
            if (ec) {
                std::cerr << "Read error: " << ec.message() << std::endl;
                break;
            }

            // Обработка запроса с помощью LambdaSenders
            bool close = false;
            beast::error_code send_ec;

            // Создаем лямбду для отправки
            LambdaSenders::send_lambda<tcp::socket> lambda(socket_, close, send_ec);

            // Обрабатываем запрос
            handler_->handleRequest(std::move(req), lambda);

            if (send_ec) {
                std::cerr << "Send error: " << send_ec.message() << std::endl;
                break;
            }

            // Если нужно закрыть соединение
            if (close) {
                break;
            }

            // Если запрос не требует keep-alive, выходим
            if (req.keep_alive()) {
                // Подготавливаем буфер для следующего запроса
                req = {};
            }
            else {
                break;
            }
        }

        // Корректное закрытие сокета
        socket_.shutdown(tcp::socket::shutdown_send, ec);
        if (ec && ec != net::error::not_connected) {
            std::cerr << "Shutdown error: " << ec.message() << std::endl;
        }
    }

    tcp::socket socket_;
    RequestHandler* handler_;
    FileCache* cache_;
    net::strand<tcp::socket::executor_type> strand_;  // Правильный тип strand
};

// Асинхронный сервер
class AsyncServer {
public:
    AsyncServer(const tcp::endpoint& endpoint,
        RequestHandler* handler,
        FileCache* cache,
        int thread_count = 1)
        : ioc_()
        , acceptor_(ioc_)
        , handler_(handler)
        , cache_(cache)
        , thread_count_(thread_count)
    {
        beast::error_code ec;

        // Открываем acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            throw std::runtime_error("Failed to open acceptor: " + ec.message());
        }

        // Устанавливаем опции
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            std::cerr << "Warning: failed to set reuse address: " << ec.message() << std::endl;
        }

        // Привязываемся к endpoint
        acceptor_.bind(endpoint, ec);
        if (ec) {
            throw std::runtime_error("Failed to bind: " + ec.message());
        }

        // Начинаем слушать
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            throw std::runtime_error("Failed to listen: " + ec.message());
        }
    }

    void run() {
        // Запускаем accept
        do_accept();

        // Запускаем потоки для обработки
        if (thread_count_ > 1) {
            threads_.reserve(thread_count_ - 1);
            for (int i = 1; i < thread_count_; ++i) {
                threads_.emplace_back([this]() {
                    try {
                        ioc_.run();
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Thread error: " << e.what() << std::endl;
                    }
                    });
            }
        }

        // Основной поток также обрабатывает запросы
        try {
            ioc_.run();
        }
        catch (const std::exception& e) {
            std::cerr << "Main thread error: " << e.what() << std::endl;
        }
    }

    void stop() {
        // Сначала останавливаем acceptor
        beast::error_code ec;
        acceptor_.close(ec);
        if (ec) {
            std::cerr << "Error closing acceptor: " << ec.message() << std::endl;
        }

        // Затем останавливаем io_context
        ioc_.stop();

        // Ждем завершения всех потоков
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](beast::error_code ec, tcp::socket socket) {
                if (!ec) {
                    // Создаем новую сессию для соединения
                    std::make_shared<Session>(
                        std::move(socket), handler_, cache_
                    )->start();

                    // Принимаем следующее соединение
                    do_accept();
                }
                else {
                    // Игнорируем operation_aborted (при остановке сервера)
                    if (ec != net::error::operation_aborted) {
                        std::cerr << "Accept error: " << ec.message() << std::endl;
                    }
                }
            });
    }

    net::io_context ioc_;
    tcp::acceptor acceptor_;
    RequestHandler* handler_;
    FileCache* cache_;
    std::vector<std::thread> threads_;
    int thread_count_;
};

int main(int argc, char* argv[]) {
    // Объявление переменных для параметров
    std::string address;
    int port;
    std::string directory;
    int threads;

    // Настройка парсера аргументов
    po::options_description desc("Available options");
    desc.add_options()
        ("help,h", "Show help")
        ("address,a", po::value<std::string>(&address)->default_value("0.0.0.0"),
            "IP address to listen on")
        ("port,p", po::value<int>(&port)->default_value(8080),
            "Port to listen on")
        ("directory,d", po::value<std::string>(&directory)->default_value("static"),
            "Path to static files")
        ("threads,t", po::value<int>(&threads)->default_value(
            std::max(1, (int)std::thread::hardware_concurrency())),
            "Number of worker threads")
        ;

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }

        if (port <= 0 || port > 65535) {
            std::cerr << "Error: port must be in the range 1-65535\n";
            return EXIT_FAILURE;
        }

        if (threads < 1) {
            std::cerr << "Error: threads must be at least 1\n";
            return EXIT_FAILURE;
        }

        // Проверка существования директории
        if (!fs::exists(directory)) {
            std::cerr << "Warning: directory '" << directory << "' does not exist\n";
        }
    }
    catch (const po::error& e) {
        std::cerr << "Argument parsing error: " << e.what() << "\n";
        std::cerr << desc << "\n";
        return EXIT_FAILURE;
    }

    // Вывод конфигурации
    std::cout << "Server configuration:\n"
        << " Address: " << address << "\n"
        << " Port: " << port << "\n"
        << " Directory: " << directory << "\n"
        << " Threads: " << threads << "\n\n";

    // Инициализация модулей
    ModuleRegistry registry;
    auto* cacheModule = registry.registerModule<FileCache>(directory.c_str(), true, 100);
    auto* requestModule = registry.registerModule<RequestHandler>();
    CreateNewHandlers(requestModule, directory);
    registry.initializeAll();

    static_cast<RequestHandler*>(requestModule)->setFileCache(cacheModule);

    try {
        auto const net_address = net::ip::make_address(address);
        auto const net_port = static_cast<unsigned short>(port);
        tcp::endpoint endpoint{ net_address, net_port };

        std::cout << "Server starting on http://" << address << ":" << port
            << " with " << threads << " threads" << std::endl;

        // Создаем и запускаем асинхронный сервер
        AsyncServer server(endpoint,
            static_cast<RequestHandler*>(requestModule),
            static_cast<FileCache*>(cacheModule),
            threads);

        // Запускаем сервер
        server.run();

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}