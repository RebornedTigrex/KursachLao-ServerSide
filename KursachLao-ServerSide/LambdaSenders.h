#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class LambdaSenders {
public:

    template<class Stream>
    struct send_lambda {
        Stream& stream_;
        bool& close_;
        beast::error_code& ec_;

        send_lambda(Stream& stream, bool& close, beast::error_code& ec)
            : stream_(stream), close_(close), ec_(ec) {
        }

        template<bool isRequest, class Body, class Fields>
        void operator()(http::message<isRequest, Body, Fields>&& msg) const {
            close_ = msg.need_eof();
            http::serializer<isRequest, Body, Fields> sr{ msg };
            http::write(stream_, sr, ec_);
        }
    };

    template<class Stream>
    struct async_send_lambda {
        Stream& stream_;
        bool& close_;

        async_send_lambda(Stream& stream, bool& close)
            : stream_(stream), close_(close) {
        }

        template<bool isRequest, class Body, class Fields>
        void operator()(http::message<isRequest, Body, Fields>&& msg) const {
            // Запоминаем, нужно ли закрывать соединение
            close_ = msg.need_eof();

            // Создаем shared_ptr, чтобы сообщение жило до завершения операции
            auto sp = std::make_shared<
                http::message<isRequest, Body, Fields>>(std::move(msg));

            // Асинхронная отправка
            http::async_write(
                stream_,
                *sp,
                [self = *this, sp, close = &close_](beast::error_code ec, std::size_t bytes) {
                    if (ec) {
                        return;
                    }

                    if (*close) {
                        beast::get_lowest_layer(self.stream_).close();
                    }
                });
        }
    };
};


