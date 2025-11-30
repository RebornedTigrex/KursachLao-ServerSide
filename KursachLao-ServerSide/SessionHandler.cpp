#include "SessionHandler.h"

#include <iostream>
#include <thread>

// Реализация метода отправки сообщений
template<class Stream>
template<bool isRequest, class Body, class Fields>
void SessionHandler::send_lambda<Stream>::operator()(
    http::message<isRequest, Body, Fields>&& msg) const
{
    close_ = msg.need_eof();
    http::serializer<isRequest, Body, Fields> sr{ msg };
    http::write(stream_, sr, ec_);
}

// Реализация обработчика запроса
template<class Body, class Allocator, class Send>
void SessionHandler::handle_request(
    http::request<Body, http::basic_fields<Allocator>>&& req,
    Send&& send)
{
    // Создаем ответ с текстом "Hello World!"
    http::response<http::string_body> res{ http::status::ok, req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/plain");
    res.keep_alive(req.keep_alive());
    res.body() = "Hello World!";
    res.prepare_payload();

    return send(std::move(res));
}

// Явные инстанциации шаблонных методов
template void SessionHandler::handle_request<
    http::string_body,
    std::allocator<char>,
    SessionHandler::send_lambda<tcp::socket>
>
    (
        http::request<http::string_body, http::basic_fields<std::allocator<char>>>&&,
        SessionHandler::send_lambda<tcp::socket>&&
    );

template class SessionHandler::send_lambda<tcp::socket>;

// Реализация обработки сессии
void SessionHandler::do_session(tcp::socket socket)
{
    bool close = false;
    beast::error_code ec;
    beast::flat_buffer buffer;

    send_lambda<tcp::socket> lambda{ socket, close, ec };

    for (;;)
    {
        // Читаем запрос
        http::request<http::string_body> req;
        http::read(socket, buffer, req, ec);
        if (ec == http::error::end_of_stream) break;
        if (ec) return;

        // Отправляем ответ "Hello World!"
        handle_request(std::move(req), lambda);
        if (ec) return;
        if (close) break;
    }

    socket.shutdown(tcp::socket::shutdown_send, ec);
}