#include "RequestHandler.h"
#include <iostream>

RequestHandler::RequestHandler()
    : BaseModule("HTTP Request Handler") {
}

bool RequestHandler::onInitialize() {
    setupDefaultRoutes();
    std::cout << "RequestHandler initialized with " << routeHandlers_.size() << " routes" << std::endl;
    if (file_cache_) {
        std::cout << "FileCache linked successfully." << std::endl;  // NEW: Лог для отладки
    }
    return true;
}

void RequestHandler::onShutdown() {
    routeHandlers_.clear();
    std::cout << "RequestHandler shutdown" << std::endl;
}

void RequestHandler::setupDefaultRoutes() {
    // Обработчик для корневого пути //TODO: Сделать проверку добавленных обработчиков. Если стандартный обработчик был добавлен, тогда бы скипаем добавление этого responce.
    /*addRouteHandler("/", [](const http::request<http::string_body>& req, http::response<http::string_body>& res) {
        res.set(http::field::content_type, "text/plain");
        res.body() = "Hello from RequestHandler module!";
        });*/
    // Обработчик для /status
    addRouteHandler("/status", [](const http::request<http::string_body>& req, http::response<http::string_body>& res) {
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"status": "ok", "service": "modular_http_server"})";
        });
}