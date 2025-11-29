#include "RequestHandler.h"
#include <iostream>

RequestHandler::RequestHandler()
    : BaseModule("HTTP Request Handler") {
}

bool RequestHandler::onInitialize() {
    setupDefaultRoutes();
    std::cout << "RequestHandler initialized with " << routeHandlers_.size() << " routes" << std::endl;
    return true;
}

void RequestHandler::onShutdown() {
    routeHandlers_.clear();
    std::cout << "RequestHandler shutdown" << std::endl;
}

void RequestHandler::addRouteHandler(const std::string& path,
    std::function<void(const http::request<http::string_body>&, http::response<http::string_body>&)> handler) {
    routeHandlers_[path] = handler;
}

void RequestHandler::setupDefaultRoutes() {
    // Обработчик для корневого пути
    addRouteHandler("/", [](const http::request<http::string_body>& req, http::response<http::string_body>& res) {
        res.set(http::field::content_type, "text/plain");
        res.body() = "Hello from RequestHandler module!";
        });

    // Обработчик для /status
    addRouteHandler("/status", [](const http::request<http::string_body>& req, http::response<http::string_body>& res) {
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"status": "ok", "service": "modular_http_server"})";
        });
}

template<class Body, class Allocator, class Send>
void RequestHandler::handleRequest(
    http::request<Body, http::basic_fields<Allocator>>&& req,
    Send&& send) {

    // Создаём ответ с базовыми заголовками
    http::response<http::string_body> res{ http::status::not_found, req.version() };
    res.set(http::field::server, (const char*)BOOST_VERSION);
    res.keep_alive(req.keep_alive());

    // Получаем целевой путь
    std::string target = std::string(req.target());

    // Ищем обработчик для этого пути
    auto it = routeHandlers_.find(target);
    if (it != routeHandlers_.end()) {
        // Вызываем найденный обработчик
        it->second(req, res);
    }
    else {
        // Маршрут не найден
        res.set(http::field::content_type, "text/plain");
        res.body() = "Route not found: " + target;
    }

    // Подготавливаем тело ответа (Content-Length, Transfer-Encoding и т.п.)
    res.prepare_payload();

    // Отправляем ответ через переданную функцию send
    send(std::move(res));
}

// Явное инстанцирование шаблона для типичного случая
template
void RequestHandler::handleRequest<
    http::string_body,
    std::allocator<char>,
    std::function<void(http::response<http::string_body>)>>(
        http::request<http::string_body, http::basic_fields<std::allocator<char>>>&& req,
        std::function<void(http::response<http::string_body>)>&& send);
