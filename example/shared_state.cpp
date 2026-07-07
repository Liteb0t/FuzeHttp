//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#include "FuzeHttpServer.hpp"
#include "PermissionObject.hpp"
#include "shared_state.hpp"
#include "WebsocketSession.hpp"
#include <boost/json/serialize.hpp>
#include <boost/dll.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/program_options.hpp>
#include <iostream>

using namespace FuzeHttp;

// shared_state::shared_state(FuzeDBI::Connection* fuze_database_interface, std::filesystem::path document_root, std::filesystem::path media_location, StateConfig config, std::unordered_map<std::string, std::string>&& busted_target_to_target, std::unordered_set<std::string>&& files_generated_from_templates)
//		: State(fuze_database_interface, std::move(busted_target_to_target), std::move(files_generated_from_templates)),
shared_state::shared_state(FuzeHttp::Server* server, StateConfig config, bool create_owner_account)
		: State(server),
		config(config),
		// fuze_dbi(server->db),
		media_location(server->media_location) {
	if (create_owner_account) {
		std::string invite_key = this->createInvite(static_cast<int>(BUILTIN_GROUPS::OWNER));
		std::cout << std::endl << "Use this link to register the owner account: http://localhost:" << this->server->server_port << "/invite/" << invite_key << std::endl;
	}
	else if (!this->ownerExists())
		std::println("\nERROR: No owner found. Restart the application with --create_owner");

}

// shared_from_this cannot be used in a constructor; see https://stackoverflow.com/questions/5558734/c-bad-weak-ptr-error
// hence a seperate start() function is used
// UPDATE 0.0.6: permission-managed objects no longer use shared pointers
void shared_state::start() {
	// Board main_board(this, db);
	// this->boards.emplace(0, main_board);
	// this->boards.at(0).cacheAllThreads();
}

std::string shared_state::getIntermediateSaltFromAccount(int account_id) {
	return db->query<std::string>("SELECT intermediate_salt_base64 FROM account WHERE id = $1", account_id);
}

const Client& shared_state::getClientFromAccountId(int account_id) const { // We assume the account with the ID is already checked
	if (!this->accounts.at(account_id).client_id)
		throw std::runtime_error(std::format("[getClientFromAccountId] No client ID assigned to account {}", account_id));
	int client_id = this->accounts.at(account_id).client_id.value();
	auto it = this->clients.find(client_id);
	if (it == this->clients.end())
		throw std::runtime_error(std::format("Account {} refers to Client {} which does not exist", account_id, client_id));
	return it->second;
}

std::string shared_state::dumpAllGroups(const std::optional<Client>& client) const {
	std::cout << "Dumping from ordered_groups_vec: ";

	boost::json::object groups_json;
	boost::json::array group_heirarchy_json;
	int group_editable_threshold;
	if (this->clientHasPermission(client, PERMISSION::MANAGE_PERMISSIONS))
		group_editable_threshold = this->getClientRank(client) + 1;
	else
		group_editable_threshold = this->getOrderedGroups()->size();
	for (int i = 0; i < this->getOrderedGroups()->size(); i++) {
		int group_id = (*(this->getOrderedGroups()))[i];
		group_heirarchy_json.emplace_back(group_id);
		std::cout << group_id << ", ";
		boost::json::object group_json{
			{"id", group_id},
			{"name", this->getGroup(group_id)->getName()},
			{"heirarchy_editable", i >= group_editable_threshold && (group_id != static_cast<int>(BUILTIN_GROUPS::USERS) && group_id != static_cast<int>(BUILTIN_GROUPS::PUBLIC))},
			{"permission_editable", i >= group_editable_threshold}
		};
		groups_json.emplace(std::to_string(group_id), group_json);
	}
	std::cout << " done." << std::endl;


	return boost::json::serialize(boost::json::object{
		{"groups", groups_json},
		{"group_heirarchy", group_heirarchy_json}
	});
}

std::string shared_state::dumpAllUsers(const std::optional<Client>& client) const {
	boost::json::object users_json;
	int client_rank = this->getClientRank(client);
	bool client_has_manage_permissions_permission = this->clientHasPermission(client, PERMISSION::MANAGE_PERMISSIONS);
	for (auto& account : this->accounts) {
		int account_id = account.first;
		std::cout << account_id << ", ";
		int account_rank = this->getAccountRank(account_id);
		boost::json::object account_json {
			{"id", account_id},
			{"username", this->getUsernameFromAccount(account_id)},
			{"rank", account_rank}
		};
		boost::json::array user_groups_json;
		for (const int group_id : this->getOrderedGroupsContainingMember(account_id)) {
			const Group* group = this->getGroup(group_id);
			user_groups_json.emplace_back(boost::json::object{
				{"id", group->getId()},
				{"name", group->getName()}
			});
		}
		account_json.emplace("groups", user_groups_json);
		account_json.emplace("permission_editable", client_has_manage_permissions_permission && client_rank < account_rank);
		users_json.emplace(std::to_string(account_id), account_json);
	}
	std::cout << " done." << std::endl;

	return boost::json::serialize(boost::json::object{
		{"users", users_json}
	});
}
