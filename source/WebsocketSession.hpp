#pragma once
//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#ifndef BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_WEBSOCKET_SESSION_HPP
#define BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_WEBSOCKET_SESSION_HPP

#include "beast.hpp"
#include "FuzeHttpState.hpp"
#include <boost/asio.hpp>
// #include <boost/hash2/sha1.hpp>
#include <cstdlib>
#include <memory>
#include <print>
#include <string>
#include <vector>

namespace FuzeHttp {

/** Represents an active WebSocket connection to the server
*/
// template<typename StateType>
class WebsocketSession : public boost::enable_shared_from_this<WebsocketSession> {
public:
	WebsocketSession(boost::asio::ip::tcp::socket&& socket, State* state)
			: ws_(std::move(socket)) , state_(state) {
	}
	~WebsocketSession() {
		// Remove this session from the list of active sessions
		state_->websocketLeave(this);
		// state_->main_board()->removeListenerFromThread(this, this->tracking_thread);
	}

	template<class Body, class Allocator>
	void run(http::request<Body, http::basic_fields<Allocator>> req) {
		// Set suggested timeout settings for the websocket
		ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

		// Set a decorator to change the Server of the handshake
		ws_.set_option(websocket::stream_base::decorator(
			[](websocket::response_type& res) {
				res.set(http::field::server,
					std::string(BOOST_BEAST_VERSION_STRING) +
						" websocket-chat-multi");
			}
		));
		this->client = this->state_->getClientIfExists(req);

		// Accept the websocket handshake
		ws_.async_accept(
			req,
			beast::bind_front_handler(
				&WebsocketSession::on_accept,
				this->shared_from_this()
			)
		);
	} // WebsocketSession::run

	// Send a message
	void send(std::shared_ptr<std::string const> const& ss) {
		// Post our work to the strand, this ensures
		// that the members of `this` will not be
		// accessed concurrently.

		boost::asio::post(
			ws_.get_executor(),
			beast::bind_front_handler(
				&WebsocketSession::on_send,
				this->shared_from_this(),
				ss
			)
		);
	}
	std::optional<Client> getClient() const { return this->client; }
protected:
	State* state_;
private:
	beast::flat_buffer buffer_;
	websocket::stream<beast::tcp_stream> ws_;
	std::optional<Client> client;
	std::vector<std::shared_ptr<std::string const>> queue_;

	void fail(beast::error_code ec, char const* what) {
		// Don't report these
		if( ec == boost::asio::error::operation_aborted ||
			ec == websocket::error::closed)
			return;

		std::cerr << what << ": " << ec.message() << "\n";
	}
	void on_accept(beast::error_code ec) {
		// Handle the error, if any
		if(ec)
			return fail(ec, "accept");

		// Add this session to the list of active sessions
		state_->websocketJoin(this);

		// db_test();

		// Read a message
		ws_.async_read(
			buffer_,
			beast::bind_front_handler(
				&WebsocketSession::on_read,
				this->shared_from_this()
			)
		);
	}
	virtual void readEvent(std::string buffer_data) {
		std::println("[FuzeHttp] [virtual readEvent] websocket buffer data: {}", buffer_data);
	}

	void on_read(beast::error_code ec, std::size_t bytes_transferred) {
		// Handle the error, if any
		if(ec)
			return fail(ec, "read");

		std::string buffer_data = beast::buffers_to_string(buffer_.data());
		this->readEvent(buffer_data);

		// Clear the buffer
		buffer_.consume(buffer_.size());

		// Read another message
		ws_.async_read(
			buffer_,
			beast::bind_front_handler(
				&WebsocketSession::on_read,
				this->shared_from_this()
			)
		);
	}
	void on_write(beast::error_code ec, std::size_t bytes_transferred) {
		// Handle the error, if any
		if(ec)
			return fail(ec, "write");

		// Remove the string from the queue
		queue_.erase(queue_.begin());

		// Send the next message if any
		if(! queue_.empty()) {
			ws_.async_write(
				boost::asio::buffer(*queue_.front()),
				beast::bind_front_handler(
					&WebsocketSession::on_write,
					this->shared_from_this()
				)
			);
		}
	}
	void on_send(std::shared_ptr<std::string const> const& ss) {
		// Always add to queue
		queue_.push_back(ss);

		// Are we already writing?
		if(queue_.size() > 1)
			return;

		// We are not currently writing, so send this immediately
		ws_.async_write(
			boost::asio::buffer(*queue_.front()),
			beast::bind_front_handler(
				&WebsocketSession::on_write,
				this->shared_from_this()
			)
		);
	}

	friend class State;
}; // class WebsocketSession

} // namespace FuzeHttp
#endif
