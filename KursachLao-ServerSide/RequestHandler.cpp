#include "RequestHandler.h"
#include <iostream>

RequestHandler::RequestHandler()
    : BaseModule("HTTP Request Handler") {
    attention_path = "..\\static\\attention.html";
    errorNotFound_path = "..\\static\\errorNotFound.html";
}

bool RequestHandler::onInitialize() {
    setupFiles();
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

