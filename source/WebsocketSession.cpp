module FuzeHttp.State:WebsocketSession;
import FuzeHttp.State;

namespace FuzeHttp {

	WebsocketSession::~WebsocketSession() {
		// Remove this session from the list of active sessions
		state_->websocketLeave(this);
		// state_->main_board()->removeListenerFromThread(this, this->tracking_thread);
	}
	template<class Body, class Allocator>
	void WebsocketSession::run(http::request<Body, http::basic_fields<Allocator>> req) {
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
	void WebsocketSession::on_accept(beast::error_code ec) {
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
}
