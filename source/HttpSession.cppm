// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include "beast.hpp"
// #include "Controller.hpp"
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <boost/optional.hpp>
#include <boost/smart_ptr.hpp>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <print>
export module FuzeHttp.HttpSession;
import FuzeHttp.Controller;
import FuzeHttp.Request;
import FuzeHttp.Response;
import FuzeHttp.State;
import FuzeHttp.Utils;


export namespace FuzeHttp {
// Represents an established HTTP connection

// Return a response for the given request.
//
// The concrete type of the response message (which depends on the
// request), is type-erased in message_generator.
template<class StateType, class WebsocketSessionType>
class HttpSession : public std::enable_shared_from_this<HttpSession<StateType, WebsocketSessionType>> {
public:
	HttpSession(boost::asio::ip::tcp::socket&& socket, StateType* state, FuzeHttp::Controller<StateType*>* controller)
			: stream_(std::move(socket)),
			state_(state),
			controller(controller) {
	}

	void run() {
		do_read();
	}

private:
	FuzeHttp::Controller<StateType*>* controller;
	beast::tcp_stream stream_;
	beast::flat_buffer buffer_;
	StateType* state_;

	// The parser is stored in an optional container so we can
	// construct it from scratch it at the beginning of each new message.
	boost::optional<http::request_parser<http::string_body>> parser_;

	struct send_lambda;
	static http::message_generator handle_request(
		StateType* state,
		FuzeHttp::Controller<StateType*>* controller,
		http::request<http::string_body, http::basic_fields<std::allocator<char>>>&& req);

	void fail(beast::error_code ec, char const* what) {
		// Don't report on canceled operations
		if(ec == boost::asio::error::operation_aborted)
			return;

		std::cerr << what << ": " << ec.message() << "\n";
	}
	void do_read() {
		// Construct a new parser for each message
		parser_.emplace();

		// Apply a reasonable limit to the allowed size
		// of the body in bytes to prevent abuse.
		parser_->body_limit(this->state_->parser_body_size_limit_mb << 20);
		// parser_->body_limit(25 << 20);

		// Set the timeout.
		stream_.expires_after(std::chrono::minutes(60));

		// Read a request
		http::async_read(
			stream_,
			buffer_,
			*parser_,
			beast::bind_front_handler(
				&HttpSession::on_read,
				this->shared_from_this()));
	}
	void on_read(beast::error_code ec, std::size_t) {
		// This means they closed the connection
		if(ec == http::error::end_of_stream) {
			stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
			return;
		}

		// Handle the error, if any
		if(ec)
			return fail(ec, "read");

		// See if it is a WebSocket Upgrade
		if(websocket::is_upgrade(parser_->get())) {
			// Create a websocket session, transferring ownership
			// of both the socket and the HTTP request.
			std::make_shared<WebsocketSessionType>(stream_.release_socket(), state_)->run(parser_->release());
			return;
		}
		else {
			// Handle request
			http::message_generator msg = handle_request(state_, controller, parser_->release());
			// http::message_generator msg = handle_request(state_->doc_root(), parser_->release());

			// Determine if we should close the connection
			bool keep_alive = msg.keep_alive();

			auto self = this->shared_from_this();

			// Send the response
			beast::async_write(
				stream_, std::move(msg),
				[self, keep_alive](beast::error_code ec, std::size_t bytes) {
					self->on_write(ec, bytes, keep_alive);
				}
			);
		}
	}
	void on_write(beast::error_code ec, std::size_t, bool keep_alive) {
		// Handle the error, if any
		if(ec)
			return fail(ec, "write");

		if(!keep_alive) {
			// This means we should close the connection, usually because
			// the response indicated the "Connection: close" semantic.
			stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
			return;
		}

		// Read another request
		do_read();
	}
};
http::message_generator createMessage(Request request, Response basic_res);
template<class StateType, class WebsocketSessionType>
http::message_generator HttpSession<StateType, WebsocketSessionType>::handle_request(
		StateType* state,
		FuzeHttp::Controller<StateType*>* controller,
		http::request<http::string_body, http::basic_fields<std::allocator<char>>>&& req_message) {
	auto client = state->getClientIfExists(req_message);
	FuzeHttp::_Request request(std::move(req_message), std::move(client));
	std::string error_text;
	try {
		FuzeHttp::Response basic_res = controller->matchPathAndExecute(state, request);
		return createMessage(request, basic_res);
	}
	catch(const std::exception& e) {
		error_text = e.what(); }
	catch (const std::string& thrown_error_text) {
		error_text = thrown_error_text; }
	catch (const char* thrown_error_text) {
		error_text = std::string(thrown_error_text); }
	// Codeflow watchamacallit reaches here if an exception was thrown
	error_text = std::format("An exception was thrown: {}", error_text);
	std::cerr << error_text << std::endl;
	Response error_res = FuzeHttp::Response{
		.status = http::status::internal_server_error,
		.error_message = error_text,
		.body = error_text
	};
	return createMessage(request, error_res);
}
} // namespace FuzeHttp
