#pragma once
#include "BaseModule.h"
#include <boost/beast/http.hpp>
#include <sstream>
#include <fstream>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;

class RequestHandler : public BaseModule {

    std::string attention_path;
    std::string attention_html;
    
    std::string errorNotFound_path;
    std::string errorNotFound_html;

    void setupFiles() {
        

        std::stringstream buffer, buffer2;
        std::ifstream file(attention_path);
        buffer << file.rdbuf();
        attention_html = buffer.str();

        std::ifstream file2(errorNotFound_path);
        buffer2 << file2.rdbuf();
        errorNotFound_html = buffer2.str();

    }

public:
    RequestHandler();

    // Методы для регистрации обработчиков конкретных путей
    void addRouteHandler(const std::string& path, std::function<void(const http::request<http::string_body>&, http::response<http::string_body>&)> handler);

    template<class Body, class Allocator, class Send>
    void handleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {

        http::response<http::string_body> res{ http::status::not_found, req.version() };
        res.set(http::field::server, "ModularServer");
        res.keep_alive(req.keep_alive());

        std::string target = std::string(req.target());
        auto it = routeHandlers_.find(target);

        if (it != routeHandlers_.end()) {
            it->second(req, res);
        }
        else if(target.find("../") != std::string::npos) {
            res.set(http::field::content_type, "text/html");
            res.body() = attention_html;
        }
        else {
            res.set(http::field::content_type, "text/html");
            res.body() = errorNotFound_html;
        }

        res.prepare_payload();
        send(std::move(res));
    }

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