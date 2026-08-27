module;
#include "beast.hpp"
// #include "FuzeHttpState.hpp"
// #include "HttpSession.hpp"
#include <boost/asio.hpp>
#include <boost/smart_ptr.hpp>
#include <iostream>
#include <memory>
#include <string>
#include "urls.hpp"
export module FuzeHttp.Listener;
import FuzeHttp.HttpSession;

export namespace FuzeHttp {
// Accepts incoming connections and launches the sessions
template<class StateType, class WebsocketSessionType>
class Listener : public std::enable_shared_from_this<Listener<StateType, WebsocketSessionType>> {
public:
	Listener(boost::asio::io_context& io_context, boost::asio::ip::tcp::endpoint endpoint, StateType* state)
			: io_context_(io_context) , acceptor_(io_context) , state_(state), controller(new FuzeHttp::Controller<StateType*>()) {
		addURLsToController(this->controller);
		beast::error_code ec;

		// Open the acceptor
		acceptor_.open(endpoint.protocol(), ec);
		if (ec) {
			fail(ec, "open");
			return;
		}

		// Allow address reuse
		acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
		if (ec) {
			fail(ec, "set_option");
			return;
		}

		// Bind to the server address
		acceptor_.bind(endpoint, ec);
		if (ec) {
			fail(ec, "bind");
			return;
		}

		// Start listening for connections
		acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);

		if (ec) {
			fail(ec, "listen");
			return;
		}
	}

	// Start accepting incoming connections
	void run() {
		// The new connection gets its own strand
		acceptor_.async_accept(
			boost::asio::make_strand(io_context_),
			beast::bind_front_handler(
				&Listener::on_accept,
				this->shared_from_this()
			)
		);
	}

private:
	FuzeHttp::Controller<StateType*>* controller;
	boost::asio::io_context& io_context_;
	boost::asio::ip::tcp::acceptor acceptor_;
	StateType* state_;

	void fail(beast::error_code ec, char const* what) {
		// Don't report on canceled operations
		if (ec == boost::asio::error::operation_aborted)
			return;
		std::cerr << what << ": " << ec.message() << "\n";
	}
	void on_accept(beast::error_code ec, boost::asio::ip::tcp::socket socket) {
		if (ec)
			return fail(ec, "accept");
		else {
			// Launch a new session for this connection
			std::make_shared<FuzeHttp::HttpSession<StateType, WebsocketSessionType>>(
				std::move(socket),
				state_,
				controller)->run();
		}

		// The new connection gets its own strand
		acceptor_.async_accept(
			boost::asio::make_strand(io_context_),
			beast::bind_front_handler(
				&Listener::on_accept,
				this->shared_from_this()
			)
		);
	}
};

}; // namespace FuzeHttp
