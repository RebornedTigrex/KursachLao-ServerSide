#ifndef SESSIONHANDLER_H
#define SESSIONHANDLER_H

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/thread.hpp>
#include <memory>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class SessionHandler {
public:
    // Обработка HTTP сессии
    static void do_session(tcp::socket socket);

private:
    // Лямбда для отправки сообщений
    template<class Stream>
    struct send_lambda {
        Stream& stream_;
        bool& close_;
        beast::error_code& ec_;

        send_lambda(Stream& stream, bool& close, beast::error_code& ec)
            : stream_(stream), close_(close), ec_(ec) {
        }

        template<bool isRequest, class Body, class Fields>
        void operator()(http::message<isRequest, Body, Fields>&& msg) const;
    };

    // Обработчик HTTP запроса - всегда возвращает "Hello World!"
    template<class Body, class Allocator, class Send>
    static void handle_request(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send);
};

#endif // SESSIONHANDLER_H