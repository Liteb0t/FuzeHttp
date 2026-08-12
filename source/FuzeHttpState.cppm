module;
// #include "FuzeHttp.hpp"
// #include "FuzeHttpServer.hpp"
// #include "FuzeHttpUtils.hpp"
#include "Request.hpp"
// #include "State_WebsocketSession_declarations.hpp"
// #include "WebsocketSession.hpp"
#include <filesystem>
#include <iostream>
#include <sodium.h>
#include <unordered_set>
export module FuzeHttp.State;
export import :WebsocketSession;
import FuzeDBI;
import FuzeHttp.Core;
import FuzeHttp.Utils;
// import State_WebsocketSession_declarations;
// import Server_declaration;

// namespace FuzeHttp {
// class WebsocketSession;
// }
export namespace FuzeHttp {
// class Server;
// template<class StateType>
// class WebsocketSession;
class State : public PermissionManager {
public:
	// State(FuzeDBI::Connection* fuze_dbi, std::unordered_map<std::string, std::string>&& busted_target_to_target, std::unordered_set<std::string>&& files_generated_from_templates);
	State(FuzeDBI::Connection* db, std::filesystem::path document_root)
		: PermissionManager(0, db), db(db), document_root(document_root) {
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
	std::optional<Client> getClientIfExists(FuzeHttp::Request req) const {
		auto cookie_header = req.find("Cookie");
		if (cookie_header == req.end())
			return {};
		std::string cookie = cookie_header->value();
		std::string session_id_base64 = cookie.substr(cookie.find("=")+1);
		// TODO trim if multiple cookies found
		std::cout << "[FuzeHttp] Received session ID: '" <<session_id_base64 << "'" << std::endl;
		if (std::unordered_map<std::string, FuzeHttp::Session>::const_iterator it = this->sessions.find(session_id_base64); it != this->sessions.end()) {
			auto client_it = this->clients.find(it->second.client_id);
			if (client_it == this->clients.end()) {
				std::print(std::cerr, "[FuzeHttp] Session ID linked to client with id {} which does not exist", it->second.client_id);
				return {};
			}
			else
				return client_it->second;
		}
		else
			return {};
	}
	Client createClient(std::optional<int> account_id = {}) {
		int new_client_id = db->query<int>("SELECT client_id FROM _sequences");
		db->query<void>("UPDATE _sequences SET client_id = $1", new_client_id+1);
		std::cout << "[FuzeHttp] Creating new client with ID " << new_client_id << std::endl;
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
		if (std::unordered_map<std::string, FuzeHttp::Invite>::const_iterator invite = this->invites.find(invite_key_base64); invite != this->invites.end())
			return invite->second.granted_group_id;
		else
			return static_cast<int>(BUILTIN_GROUPS::PUBLIC);
	}
	void clearExpiredSessions() {
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
	const char* getSecret() const { return this->secret_base64; }
	void websocketJoin (FuzeHttp::WebsocketSession* session) {
		std::lock_guard<std::mutex> lock(mutex_);
		websocket_sessions.insert(session);
	}
	virtual void websocketRead (FuzeHttp::WebsocketSession* session) {}
	void websocketLeave(FuzeHttp::WebsocketSession* session) {
		std::lock_guard<std::mutex> lock(mutex_);
		websocket_sessions.erase(session);
	}
	// const std::unordered_map<std::string, std::string> busted_target_to_target;
	// const std::unordered_set<std::string> files_generated_from_templates;
	// virtual void start() {};
	// FuzeHttp::Server* server;
	FuzeDBI::Connection* db;
protected:
	const std::optional<Client> getClientFromSession(const std::string& session_id_base64) const {
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
	std::filesystem::path document_root;
	// std::unordered_map<std::filesystem::path, std::string> document_etags;
	// FuzeDBI::Connection* fuze_dbi;
	std::vector<FuzeHttp::TemplateMacro*> options;
	// This mutex synchronizes all access to websocket_sessions
	std::mutex mutex_;
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
		for (auto client_tuple : db->queryRows<std::tuple<int, int>>("SELECT id, account_id FROM client")) {
			std::optional<int> account_id;
			if (std::get<1>(client_tuple) != -1)
				account_id = std::get<1>(client_tuple);
			else
				account_id = {};
			Client client{
				.id = std::get<0>(client_tuple),
				.account_id = account_id
			};
			this->clients.emplace(std::get<0>(client_tuple), std::move(client));
		}
	}
	const std::chrono::duration<unsigned int> authorization_token_lifespan = std::chrono::days(365);
	char secret_base64[sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)];
	// friend class FuzeHttp::Server;
	friend class WebsocketSession;
}; // class State
} // namespace FuzeHttp
