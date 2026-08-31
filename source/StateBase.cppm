module;
// #include "FuzeHttp.hpp"
// #include "FuzeHttpServer.hpp"
// #include "FuzeHttpUtils.hpp"
#include "Request.hpp"
// #include "State_WebsocketSession_declarations.hpp"
// #include "WebsocketSession.hpp"
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <filesystem>
#include <iostream>
#include <list>
#include <print>
#include <sodium.h>
#include <unordered_set>
export module FuzeHttp.State;
import FuzeDBI;
import FuzeHttp.Core;
import FuzeHttp.Migrations;
import FuzeHttp.Utils;
// export import :WebsocketSession;
// import State_WebsocketSession_declarations;
// import Server_declaration;
/*
namespace FuzeHttp {
class WebsocketSession;
}*/
export namespace FuzeHttp {
// class Server;
// template<class StateType>
class WebsocketSession; // forward declaration
// WebsocketSession is placed in this module to workaround a circular dependency issue which was an obstacle to module migration. TODO: separate WebsocketSession into its own module or partition if possible.

class StateBase : public PermissionManager {
public:
	StateBase(FuzeDBI::Connection* db) : PermissionManager(0, db), db(db) {
		this->loadSessions();
		this->loadClients();
		// Link accounts to clients
		for (const auto& client_pair : this->clients) {
			if (client_pair.second.account_id) {
				int account_id = client_pair.second.account_id.value();
				auto it = this->accounts.find(account_id);
				if (it == this->accounts.end())
					throw std::runtime_error(std::format("Client {} refers to account {} which does not exist", client_pair.first, account_id));
				it->second.client_id = client_pair.first;
				std::cout << "Account " <<account_id << " = Client " <<client_pair.first << std::endl;
			}
		}
	}
	virtual void start() {} // called after options set
	virtual std::list<std::unique_ptr<Migrations::Migration>> addMigrations() {
		std::list<std::unique_ptr<Migrations::Migration>> migrations;
		/* Example migrations:
		migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.1.1",
			"ALTER TABLE message_file ADD COLUMN width INTEGER;"
			"ALTER TABLE message_file ADD COLUMN height INTEGER;")));
		migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.1.2",
			"ALTER TABLE message_file ADD COLUMN thumbnail_file_extension TEXT;"
			"ALTER TABLE thread ADD COLUMN message_id_seq INTEGER DEFAULT 0;"
			"UPDATE thread SET message_id_seq = 1000")));
		migrations.push_back(std::unique_ptr<Migration>(new SmartMigration("0.2.2", state, [](FuzeDBI::Connection* db, shared_state* state){
			const std::string version_string = db->query<std::string>("SELECT version FROM _info");
			std::println("This is the lambda and document_root is {} and version string is {}", state->getDocumentRoot().string(), version_string);
		})));
		*/
		return migrations;
	}
	std::optional<Client> getClientIfExists(FuzeHttp::Request req) const {
		auto cookie_header = req.find("Cookie");
		if (cookie_header == req.end())
			return {};
		std::string cookie = cookie_header->value();
		std::string session_id_base64 = cookie.substr(cookie.find("=")+1);
		// TODO trim if multiple cookies found
		std::cout << "[FuzeHttp] Received session ID: '" <<session_id_base64 << "'" << std::endl;
		std::lock_guard<std::mutex> lock(mutex);
		if (std::unordered_map<std::string, FuzeHttp::Session>::const_iterator it = this->sessions.find(session_id_base64); it != this->sessions.end()) {
			auto client_it = this->clients.find(it->second.client_id);
			if (client_it == this->clients.end()) {
				std::print(std::cerr, "[FuzeHttp] Session ID linked to client with id {} which does not exist", it->second.client_id);
				return {};
			}
			else {
				std::println("Client ID: {}", client_it->second.id);
				if (client_it->second.account_id)
					std::println("----Account ID: {}", client_it->second.account_id.value());
				return client_it->second;
			}
		}
		else
			return {};
	}
	Client createClient(std::optional<int> account_id = {}) {
		int new_client_id = db->incrementSequence("client_id");
		std::cout << "[FuzeHttp] Creating new client with ID " << new_client_id << std::endl;
		std::lock_guard<std::mutex> lock(mutex);
		if (account_id) {
			db->query<void>("INSERT INTO client(id, account_id) VALUES ($1, $2)", new_client_id, account_id.value());
			this->accounts.at(account_id.value()).client_id = new_client_id;
		}
		else
			db->query<void>("INSERT INTO client(id) VALUES ($1)", new_client_id);
		Client client{.id = new_client_id, .account_id = account_id};
		this->clients.emplace(new_client_id, client);
		return client;
	}
	// std::variant<Client, FuzeHttp::Response> getRequiredClient(FuzeHttp::Request req) const;
	std::string createSession(int client_id) {
		std::lock_guard<std::mutex> lock(mutex);
		FuzeHttp::Session session{
			.client_id = client_id,
			.created_at = std::chrono::system_clock::now()
		};
		std::string key_base64 = generateKeyBase64(this->sessions);
		db->query<void>("INSERT INTO session(client_id, key, created_at) VALUES ($1, $2, $3)", client_id, key_base64, (int)std::chrono::duration_cast<std::chrono::seconds>(session.created_at.time_since_epoch()).count());
		// db->createSession(
		// 	key_base64,
		// 	session.client_id,
		// 	std::chrono::duration_cast<std::chrono::minutes>(session.created_at.time_since_epoch()).count()
		// );
		this->sessions.emplace(key_base64, std::move(session));
		return key_base64;
	}
	std::string createInvite(int granted_group_id) {
		std::lock_guard<std::mutex> lock(mutex);
		FuzeHttp::Invite invite{
			.granted_group_id = granted_group_id,
			.created_at = std::chrono::system_clock::now()
		};
		std::string key_base64 = FuzeHttp::generateKeyBase64(this->sessions);
		// TODO save invite to database
		std::cout << "[shared_state] Created invite with key " << key_base64 << std::endl;
		this->invites.emplace(key_base64, std::move(invite));
		return key_base64;
	}
	int getGrantedGroupIdFromInvite(const std::string& invite_key_base64) const { // returns PUBLIC if none found
		std::lock_guard<std::mutex> lock(mutex);
		if (std::unordered_map<std::string, FuzeHttp::Invite>::const_iterator invite = this->invites.find(invite_key_base64); invite != this->invites.end())
			return invite->second.granted_group_id;
		else
			return static_cast<int>(BUILTIN_GROUPS::PUBLIC);
	}
	void clearExpiredSessions() {
		std::lock_guard<std::mutex> lock(mutex);
		int initial_number_of_sessions = this->sessions.size();
		std::chrono::time_point<std::chrono::system_clock> current_time = std::chrono::system_clock::now();
		std::erase_if(this->sessions, [this, &current_time](const std::pair<std::string, FuzeHttp::Session>& session_pair){
			if (session_pair.second.created_at + this->authorization_token_lifespan < current_time) {
				db->query<void>("DELETE FROM session WHERE key = $1", session_pair.first);
				return true;
			}
			else
				return false;
		});
		std::cout << "[shared_state] Cleared " << initial_number_of_sessions - this->sessions.size() << " expired sessions." << std::endl;
	}
	const std::filesystem::path& getDocumentRoot() const { return document_root; }
	const std::string getSecret() const { return this->secret_base64; }
	void websocketJoin (FuzeHttp::WebsocketSession* session) {
		std::lock_guard<std::mutex> lock(mutex);
		websocket_sessions.insert(session);
	}
	virtual void websocketRead (FuzeHttp::WebsocketSession* session) {}
	void websocketLeave(FuzeHttp::WebsocketSession* session) {
		std::lock_guard<std::mutex> lock(mutex);
		websocket_sessions.erase(session);
	}
	void setSecretFromEnvironmentVariable(const std::string& environment_variable_for_secret, bool secret_required) {
		std::lock_guard<std::mutex> lock(mutex);
		if (const unsigned char* secret_pointer = reinterpret_cast<const unsigned char*>(std::getenv(environment_variable_for_secret.c_str()))) {
			char secret_base64_a[sodium_base64_ENCODED_LEN(sizeof secret_pointer, sodium_base64_VARIANT_URLSAFE)];
			sodium_bin2base64(
				secret_base64_a, sizeof secret_base64_a,
				secret_pointer, sizeof secret_pointer,
				sodium_base64_VARIANT_URLSAFE
			);
			this->secret_base64 = secret_base64_a;
		}
		else if (secret_required)
			throw std::runtime_error(std::format("Environment variable {} not found. Specify `environment_variable_for_secret` OR set secret_required=false", environment_variable_for_secret));
		else
			std::println("Warning: Environment variable {} not found. Continuing anyway because secret_required is set to false.", environment_variable_for_secret);
	}
	std::string getIntermediateSaltFromAccount(int account_id) const {
		return db->query<std::string>("SELECT intermediate_salt_base64 FROM account WHERE id = $1", account_id);
	}
	const FuzeHttp::Client getClientFromAccountId(int account_id) const { // We assume the account with the ID is already checked
		std::lock_guard<std::mutex> lock(mutex);
		if (!this->accounts.at(account_id).client_id)
			throw std::runtime_error(std::format("[getClientFromAccountId] No client ID assigned to account {}", account_id));
		int client_id = this->accounts.at(account_id).client_id.value();
		auto it = this->clients.find(client_id);
		if (it == this->clients.end())
			throw std::runtime_error(std::format("Account {} refers to Client {} which does not exist", account_id, client_id));
		std::println("From account {} found client {}", account_id, it->second.id);
		return it->second;
	}
	// const std::unordered_map<std::string, std::string> busted_target_to_target;
	// const std::unordered_set<std::string> files_generated_from_templates;
	// virtual void start() {};
	// FuzeHttp::Server* server;
	FuzeDBI::Connection* db;
	std::filesystem::path document_root;
	std::filesystem::path media_location;
	std::unordered_map<std::string /*target*/, std::string /*etag*/> manifest_frontend_etags;
	std::unordered_map<std::string, std::string> busted_target_to_target;
	std::unordered_set<std::string> files_generated_from_templates;
	std::string frontend_etag; // Changes when any frontend file changes, ensuring client refreshes cache.
	unsigned int parser_body_size_limit_mb;
protected:
	const std::optional<Client> getClientFromSession(const std::string& session_id_base64) const {
		std::lock_guard<std::mutex> lock(mutex);
		if (std::unordered_map<std::string, FuzeHttp::Session>::const_iterator session = this->sessions.find(session_id_base64); session != this->sessions.end())
			return this->clients.at(session->second.client_id);
		else
			return {};
	}

	// void createOwnerAccount(DatabaseConnection* db, const std::string& username, const std::string& password);

	// std::variant<http::file_body::value_type, FuzeHttp::Response> openFile(std::filesystem::path path) const;


	// virtual void onWebsocketJoin  (WebsocketSession* session) {}
	// virtual void onWebsocketLeave (WebsocketSession* session) {}
	std::unordered_map<int, Client> clients;
	std::unordered_map<std::string /*key_base64*/, Session> sessions;
	std::unordered_map<std::string /*key_base64*/, Invite> invites;
	// Keep a list of all the websocket-connected clients
	std::unordered_set<FuzeHttp::WebsocketSession*> websocket_sessions;
	// std::unordered_map<std::filesystem::path, std::string> document_etags;
	// FuzeDBI::Connection* fuze_dbi;
	std::vector<FuzeHttp::TemplateMacro*> options;
	// This mutex synchronizes all access to websocket_sessions
	mutable std::mutex mutex;
private:
	void loadSessions() {
		for (auto session_tuple : db->queryRows<std::tuple<int, std::string, int>>("SELECT client_id, key, created_at FROM session")) {
			int seconds_since_epoch = std::get<2>(session_tuple); // TODO use long instead of int
			std::chrono::seconds sec(seconds_since_epoch);
			std::chrono::time_point<std::chrono::system_clock> created_at(sec);
			FuzeHttp::Session session{
				.client_id = std::get<0>(session_tuple),
				.created_at = created_at
			};
			this->sessions.emplace(std::get<1>(session_tuple), std::move(session));
		}
	}
	void loadClients() {
		// TODO clear clients which have expired or dont have an account
		for (auto client_tuple : db->queryRows<std::tuple<int, std::optional<int>>>("SELECT id, account_id FROM client")) {
			Client client{
				.id = std::get<0>(client_tuple),
				.account_id = std::get<1>(client_tuple)
			};
			this->clients.emplace(std::get<0>(client_tuple), std::move(client));
		}
	}
	const std::chrono::duration<unsigned int> authorization_token_lifespan = std::chrono::days(365);
	std::string secret_base64;
	// friend class FuzeHttp::Server;
	friend class WebsocketSession;
}; // class State

class WebsocketSession : public std::enable_shared_from_this<WebsocketSession> {
public:
	WebsocketSession(boost::asio::ip::tcp::socket&& socket, StateBase* state)
			: ws_(std::move(socket)) , state_(state) {
	}
	~WebsocketSession();

	template<class Body, class Allocator>
	void run(http::request<Body, http::basic_fields<Allocator>> req);

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
	void send(const std::string& data) {
		this->send(std::make_shared<const std::string>(data));
	}
	void send(const boost::json::object& json) {
		this->send(std::make_shared<const std::string>(boost::json::serialize(json)));
	}
	std::optional<Client> getClient() const { return this->client; }
protected:
	StateBase* state_;
private:
	beast::flat_buffer buffer_;
	websocket::stream<beast::tcp_stream> ws_;
	std::optional<Client> client;
	std::vector<std::shared_ptr<const std::string>> queue_;

	void fail(beast::error_code ec, char const* what) {
		// Don't report these
		if( ec == boost::asio::error::operation_aborted ||
			ec == websocket::error::closed)
			return;

		std::cerr << what << ": " << ec.message() << "\n";
	}
	void on_accept(beast::error_code ec);
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
	void on_send(std::shared_ptr<const std::string> const& ss) {
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

	friend class StateBase;
}; // class WebsocketSession
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
} // namespace FuzeHttp
