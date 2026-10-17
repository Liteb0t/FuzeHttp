module;
#include "beast.hpp"
#include "boost/json.hpp"
#include <iostream>
#include <filesystem>
#include <print>
module FuzeHttp.HttpSession;
import FuzeHttp.Controller;
import FuzeHttp.Request;
import FuzeHttp.Response;
import FuzeHttp.Utils;

namespace FuzeHttp {
http::response<http::string_body> buildStringResponse(FuzeHttp::Response basic_response, Request req) {
	http::response<http::string_body> res{basic_response.status, req.message.version()};
	// res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
	for (auto& header : basic_response.headers)
		res.set(header.first, header.second);
	if (basic_response.error_message)
		res.set("message", basic_response.error_message.value());
	if (basic_response.json) {
		res.set(http::field::content_type, "application/json");
		res.body() = boost::json::serialize(basic_response.json.value());
	}
	else if (basic_response.error_message)
		res.body() = basic_response.error_message.value();
	else if (basic_response.body)
		res.body() = basic_response.body.value();
	res.keep_alive(req.message.keep_alive());
	res.prepare_payload();
	return res;
}

http::response<http::file_body> buildFileResponse(FuzeHttp::Response basic_response, Request req) {
	// std::println("Attempting to open file {}", basic_response.file.value());
	// Attempt to open the file
	beast::error_code ec;
	http::file_body::value_type body;
	body.open(basic_response.file.value().c_str(), beast::file_mode::scan, ec);

	if (ec) // Handle an unknown error
		throw std::runtime_error("Unknown error when attempting to open file");

	// Cache the size since we need it after the move
	const uint64_t size = body.size();

	http::response<http::file_body> res{
		std::piecewise_construct,
		std::make_tuple(std::move(body)),
		std::make_tuple(http::status::ok, req.message.version())
	};
	for (auto& header : basic_response.headers)
		res.set(header.first, header.second);
	// res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
	res.set(http::field::content_type, getMimeType(basic_response.file.value().string()));
	res.content_length(size);
	res.keep_alive(req.message.keep_alive());
	return res;
}

http::response<http::empty_body> buildEmptyResponse(FuzeHttp::Response basic_response, Request req) {
	http::response<http::empty_body> res{basic_response.status, req.message.version()};
	// res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
	for (auto& header : basic_response.headers)
		res.set(header.first, header.second);
	if (basic_response.error_message)
		res.set("message", basic_response.error_message.value());
	res.keep_alive(req.message.keep_alive());
	res.prepare_payload();
	return res;
}

http::message_generator createMessage(Request request, Response basic_res) {
	// Matches paths in urls.cpp
	std::string error_text;
	std::cout << "[HttpSession] basic_res.status: " << basic_res.status << std::endl;
	if (basic_res.error_message)
		std::println("Error message: {}", basic_res.error_message.value());
	if (basic_res.json || basic_res.body)
		return buildStringResponse(basic_res, request);
	else if (basic_res.file) {
		// if (!std::filesystem::is_regular_file(basic_res.file.value()))
		// 	basic_res.file = basic_res.file.value() / "index.html";
		std::print("Checking if file exists: {}", basic_res.file.value().string());
		if (!std::filesystem::exists(basic_res.file.value())) {
			std::println(" - NOT FOUND");
			throw std::format("Target not found: `{}`", std::string(request.message.target()));
		}
		else {
			std::println(" - it exists :D");
			return buildFileResponse(basic_res, request);
		}
	}
	else
		return buildEmptyResponse(basic_res, request);
}
} // namespace FuzeHttp
