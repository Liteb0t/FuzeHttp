//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#ifndef BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_SHARED_STATE_HPP
#define BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_SHARED_STATE_HPP

#include "beast.hpp"
// #include "FuzeHttpState.hpp"
// #include "PermissionObject.hpp"
#include <boost/smart_ptr.hpp>
#include <filesystem>
#include <list>
#include <mutex>
#include <string>
#include <unordered_set>
import FuzeHttp.Migrations;
import FuzeHttp.PermissionObject;
import FuzeHttp.State;
import FuzeDBI;

// Forward declaration
class WebsocketSession;
struct StateConfig {
	std::string server_name;
};

// Represents the shared server state
class shared_state : public FuzeHttp::StateBase {
public:
	shared_state(FuzeDBI::Connection* db) : StateBase(db) {}
	// shared_state(StateConfig config) : Stateconfig(config) {}
	// shared_state(FuzeDBI::Connection* fuze_database_interface, std::filesystem::path document_root, std::filesystem::path media_location_relative, StateConfig config, std::unordered_map<std::string, std::string>&& busted_target_to_target, std::unordered_set<std::string>&& files_generated_from_templates);
	StateConfig config;
	void start() override;
	std::list<std::unique_ptr<FuzeHttp::Migrations::Migration>> addMigrations() override;

	// FuzeDBI::Connection* fuze_dbi;

	const int client_pwhash_opslimit = 2; // CPU cost for client-side password hashing.
	const int client_pwhash_memlimit = 128 << 20; // Likewise, memory cost.

	std::string dumpAllGroups(const std::optional<FuzeHttp::Client>& client) const;
	std::string dumpAllUsers(const std::optional<FuzeHttp::Client>& client) const;
	std::string getIntermediateSaltFromAccount(int account_id);
	const FuzeHttp::Client& getClientFromAccountId(int account_id) const;

	void clearWebsockets();

	const std::filesystem::path& getMediaLocation() const { return media_location; }
	// const std::filesystem::path& getProgramLocation() const { return program_location; }
private:
	// const std::filesystem::path program_location;
};

#endif
