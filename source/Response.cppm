// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <boost/beast/http/status.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
export module FuzeHttp.Response;

using namespace boost;

export namespace FuzeHttp {
struct Response {
	beast::http::status status;
	std::unordered_map<std::string, std::string> headers;
	std::optional<std::string> error_message;
	std::optional<boost::json::value> json;
	std::optional<std::filesystem::path> file;
	std::optional<std::string> body;
};
using Headers = std::unordered_map<std::string, std::string>;
} // export namespace FuzeHttp
