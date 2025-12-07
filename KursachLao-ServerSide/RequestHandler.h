#pragma once
#include "BaseModule.h"
#include "FileCache.h"
#include <boost/beast/http.hpp>
#include <sstream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <functional>
#include <variant>  // NEW: Для поддержки разных типов обработчиков

namespace beast = boost::beast;
namespace http = beast::http;

class RequestHandler : public BaseModule {
    FileCache* file_cache_ = nullptr;

    // NEW: Типы обработчиков
    using SyncHandler = std::function<void(const http::request<http::string_body>&,
        http::response<http::string_body>&)>;

    template<typename Send>
    using AsyncHandler = std::function<void(const http::request<http::string_body>&,
        Send&& send)>;

    // NEW: Вариант для хранения разных типов обработчиков
    struct HandlerVariant {
        std::variant<SyncHandler> handler;

        template<typename Send>
        void execute(const http::request<http::string_body>& req, Send&& send) const {
            if (auto* sync = std::get_if<SyncHandler>(&handler)) {
                http::response<http::string_body> res{ http::status::ok, req.version() };
                res.set(http::field::server, "ModularServer");
                res.keep_alive(req.keep_alive());
                (*sync)(req, res);
                res.prepare_payload();
                send(std::move(res));
            }
        }
    };

public:
    RequestHandler();

    void setFileCache(FileCache* cache) {
        file_cache_ = cache;
        if (file_cache_) {
            std::string base_dir = file_cache_->get_base_directory();
            // Можно добавить логику инициализации с кэшем
        }
    }

    // Существующий метод для синхронных обработчиков
    void addRouteHandler(const std::string& path, SyncHandler handler) {
        routeHandlers_[path] = HandlerVariant{ std::move(handler) };
    }

    // NEW: Метод для асинхронных обработчиков
    template<typename SendFunc>
    void addAsyncRouteHandler(const std::string& path,
        std::function<void(const http::request<http::string_body>&,
            SendFunc&&)> handler) {
        // Для простоты пока используем только синхронные
        // Можно расширить HandlerVariant для поддержки асинхронных
        addRouteHandler(path, [h = std::move(handler)](const auto& req, auto& res) {
            // Пока преобразуем асинхронный в синхронный
            // В реальности нужно будет переделать архитектуру
            });
    }

    template<class Body, class Allocator, class Send>
    void handleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target = std::string(req.target());
        auto [path, query] = parseTarget(target);

        // Проверяем точные маршруты
        auto it = routeHandlers_.find(path);
        if (it != routeHandlers_.end()) {
            it->second.execute(req, std::forward<Send>(send));
            return;
        }

        // Проверяем wildcard для статических файлов
        auto wildcard_it = routeHandlers_.find("/*");
        if (wildcard_it != routeHandlers_.end() && file_cache_) {
            serveStaticFile(path, std::move(req), std::forward<Send>(send));
            return;
        }

        // Обработка ошибок
        serveError(path, std::move(req), std::forward<Send>(send));
    }

private:
    std::pair<std::string, std::string> parseTarget(const std::string& target) {
        size_t pos = target.find('?');
        if (pos == std::string::npos) {
            return { target, "" };
        }
        return { target.substr(0, pos), target.substr(pos + 1) };
    }

    template<class Body, class Allocator, class Send>
    void serveStaticFile(const std::string& path,
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send) {
        http::response<http::string_body> res{ http::status::ok, req.version() };
        res.set(http::field::server, "ModularServer");
        res.keep_alive(req.keep_alive());

        file_cache_->refresh_file(path);
        auto cached_file = file_cache_->get_file(path);

        if (cached_file) {
            res.set(http::field::content_type, cached_file->mime_type.c_str());
            res.body() = std::move(cached_file->content);
            res.result(http::status::ok);
        }
        else {
            res.result(http::status::not_found);
            res.set(http::field::content_type, "text/html");
            auto error_file = file_cache_->get_file("/errorNotFound");
            if (error_file) {
                res.body() = error_file->content;
            }
            else {
                res.body() = "File not found";
            }
        }

        res.prepare_payload();
        send(std::move(res));
    }

    template<class Body, class Allocator, class Send>
    void serveError(const std::string& path,
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send) {
        http::response<http::string_body> res{ http::status::not_found, req.version() };
        res.set(http::field::server, "ModularServer");
        res.keep_alive(req.keep_alive());

        if (path.find("../") != std::string::npos) {
            res.set(http::field::content_type, "text/html");
            if (file_cache_) {
                file_cache_->refresh_file("/attention");
                auto cached = file_cache_->get_file("/attention");
                if (cached) res.body() = cached->content;
            }
        }
        else {
            res.set(http::field::content_type, "text/html");
            if (file_cache_) {
                file_cache_->refresh_file("/errorNotFound");
                auto cached = file_cache_->get_file("/errorNotFound");
                if (cached) res.body() = cached->content;
            }
        }

        if (res.body().empty()) {
            res.body() = "Error";
        }

        res.prepare_payload();
        send(std::move(res));
    }

    bool onInitialize() override;
    void onShutdown() override;

    std::unordered_map<std::string, HandlerVariant> routeHandlers_;
    void setupDefaultRoutes();
};