#pragma once

#include<boost/beast.hpp>

using file_body = boost::beast::http::file_body;
using string_body = boost::beast::http::string_body;

using fRequest = boost::beast::http::request<file_body>;
using sRequest = boost::beast::http::request<string_body>;

using fResponce = boost::beast::http::response<file_body>;
using sResponce = boost::beast::http::response<string_body>;