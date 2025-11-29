#pragma once
#include "BaseModule.h"
#include <boost/beast/http.hpp>

namespace beast = boost::beast;
namespace http = beast::http;

class RequestHandler : public BaseModule {
public:
    RequestHandler();

    template<class Body, class Allocator, class Send>
    void handleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send);

    // Методы для регистрации обработчиков конкретных путей
    void addRouteHandler(const std::string& path, std::function<void(const http::request<http::string_body>&, http::response<http::string_body>&)> handler);

protected:
    bool onInitialize() override;
    void onShutdown() override;

private:
    std::unordered_map<
        std::string,
        std::function<void(const http::request<http::string_body>&, http::response<http::string_body>&)>
    > routeHandlers_;

    void setupDefaultRoutes();
};