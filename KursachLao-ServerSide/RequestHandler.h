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
    std::string attention_path;
    std::string attention_html;

    std::string errorNotFound_path;
    std::string errorNotFound_html;
    FileCache* file_cache_ = nullptr;  // NEW: Указатель на кэш (инжектируется в main)

    void setupBasicHtmlPages() {
        std::stringstream buffer, buffer2;
        std::ifstream file(attention_path);
        buffer << file.rdbuf();
        attention_html = buffer.str();
        std::ifstream file2(errorNotFound_path);
        buffer2 << file2.rdbuf();
        errorNotFound_html = buffer2.str();
    }

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
    void setFileCache(FileCache* cache) { file_cache_ = cache; }

    // Методы для регистрации обработчиков конкретных путей
    void addRouteHandler(const std::string& path, std::function<void(const http::request<http::string_body>&, http::response<http::string_body>&)> handler);

    template<class Body, class Allocator, class Send>
    void handleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        http::response<http::string_body> res{ http::status::not_found, req.version() };
        res.set(http::field::server, "ModularServer");
        res.keep_alive(req.keep_alive());

        std::string target = std::string(req.target());
        auto [path, query] = parseTarget(target);  // NEW: Парсим path и query

        // UPDATED: Проверяем wildcard /* для динамического поиска в кэше (только по path!)
        auto wildcard_it = routeHandlers_.find("/*");
        if (wildcard_it != routeHandlers_.end() && file_cache_ && path != "/") {
            auto cached_file = file_cache_->get_file(path);  // Ищем по чистому path
            if (cached_file) {
                res.set(http::field::content_type, cached_file->mime_type.c_str());
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
            it->second(req, res);  // Пока без query (для /test, /status)
        }
        else if (target.find("../") != std::string::npos) {  // UPDATED: Проверяем полный target (с query)
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