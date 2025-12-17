#pragma once
#include "BaseModule.h"
#include "FileCache.h"  // NEW: Для доступа к кэшу

#include <boost/beast/http.hpp>
#include <sstream>
#include <fstream>
#include <vector>
#include <unordered_map>  // UPDATED: Для routeHandlers_

namespace beast = boost::beast;
namespace http = beast::http;

class RequestHandler : public BaseModule {
    FileCache* file_cache_ = nullptr;  // NEW: Указатель на кэш (инжектируется в main)


    // NEW: Парсинг target на path и query (простой split по ?)
    std::pair<std::string, std::string> parseTarget(const std::string& target) {
        size_t pos = target.find('?');
        if (pos == std::string::npos) {
            return { target, "" };  // Нет query
        }
        return { target.substr(0, pos), target.substr(pos + 1) };  // path, query
    }

public:
    RequestHandler();
    // NEW: Метод для инжекции кэша (только из main)
    void setFileCache(FileCache* cache) {
        file_cache_ = cache;
        std::string base_dir = file_cache_->get_base_directory();

    }
    // Методы для регистрации обработчиков конкретных путей
    void addRouteHandler(const std::string& path, std::function<void(const http::request<http::string_body>&, http::response<http::string_body>&)> handler);

    template<class Body, class Allocator, class Send>
    void handleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        http::response<http::string_body> res{ http::status::not_found, req.version() };
        res.set(http::field::server, "ModularServer");
        // FIXED: Uncomment и используй req.keep_alive() — mirror client
        res.keep_alive(req.keep_alive());
        // NEW: Explicit Connection header для force keep-alive (если !reиq.keep_alive(), но для MVP — всегда true для 1.1)
        if (req.version() >= 11 && res.keep_alive()) {
            res.set(http::field::connection, "keep-alive");
        }

        std::string target = std::string(req.target());
        auto [path, query] = parseTarget(target);

        // UPDATED: Проверяем wildcard /* для динамического поиска в кэше (только по path!)
        auto wildcard_it = routeHandlers_.find("/*"); //FIXME: Повышает время отклика на (n)
        if (wildcard_it != routeHandlers_.end() && file_cache_) {
            file_cache_->refresh_file(path);
            auto cached_file = file_cache_->get_file(path);  // Ищем по чистому path
            if (cached_file) {
                res.set(http::field::content_type, cached_file->mime_type.c_str());
                res.set(http::field::cache_control, "public, max-age=300");
                res.body() = std::move(cached_file->content);
                res.result(http::status::ok);
                res.prepare_payload();
                send(std::move(res));
                return;
            }
        }

        // UPDATED: Обычная логика для точных маршрутов (match по path)
        auto it = routeHandlers_.find(path);
        if (it != routeHandlers_.end()) {
            // NEW: Передаём query в handler (если lambda ожидает — расширь signature)
            // Для MVP: если handler статический, игнорируем query
            // Пример: it->second(req, res, query);  // Если изменишь lambda на void(const sRequest&, sResponce&, const std::string& query)
            it->second(req, res); 
        }
        else if (target.find("../") != std::string::npos) {
            res.set(http::field::content_type, "text/html");
            file_cache_->refresh_file("/attention");
            const auto& cached = file_cache_->get_file("/attention");
            res.set(http::field::cache_control, "public, max-age=300");
            res.body() = cached.value().content;
        }
        else if (target.find("api/") != std::string::npos) {
            res.set(http::field::content_type, "application/json");
            res.result(http::status::not_found);
            res.set(http::field::cache_control, "no-cache, must-revalidate");
            res.body() = R"({"status": "not_found"})";
        }
        else {
            res.set(http::field::content_type, "text/html");
            file_cache_->refresh_file("/errorNotFound");
            const auto& cached = file_cache_->get_file("/errorNotFound");
            res.set(http::field::cache_control, "public, max-age=300");
            res.body() = cached.value().content;
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