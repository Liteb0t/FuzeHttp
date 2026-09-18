// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include "beast.hpp"
#include <boost/json.hpp>
#include <boost/smart_ptr.hpp>
#include <filesystem>
#include <list>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_set>
export module FuzeHttp.Example.State;
import FuzeHttp.Migrations;
import FuzeHttp.PermissionObject;
import FuzeHttp.State;
import FuzeDBI;

export namespace FuzeHttp {
namespace Example {
struct StateConfig {
	std::string server_name;
};
struct TestObject {
	int child_object_id;
	std::string name = "this is a test object.";
};
struct TestChildObject {
	std::string name = "this is a child object.";
};
enum struct PERMISSION : int {
	MANAGE_PERMISSIONS,
	VIEW_THREAD,
	CREATE_THREAD,
	SEND_MESSAGE,
	DELETE_POST,
	UPLOAD_FILE,
	NUMBER_OF_PERMISSIONS
};


// Represents the shared server state
class shared_state : public FuzeHttp::StateBase {
public:
	shared_state(FuzeDBI::Connection* db) : StateBase(db) {
		for (int i = 0; i < 10; ++i) {
			// objects is std::unordered_map<int, std::shared_ptr<TestObject>>
			this->objects.emplace(i, std::make_shared<TestObject>(TestObject{.child_object_id = i}));
			this->child_objects.emplace(i, std::make_shared<TestChildObject>());
		}
	}
	// shared_state(StateConfig config) : Stateconfig(config) {}
	// shared_state(FuzeDBI::Connection* fuze_database_interface, std::filesystem::path document_root, std::filesystem::path media_location_relative, StateConfig config, std::unordered_map<std::string, std::string>&& busted_target_to_target, std::unordered_set<std::string>&& files_generated_from_templates);
	StateConfig config;
	// FuzeDBI::Connection* fuze_dbi;

	const int client_pwhash_opslimit = 2; // CPU cost for client-side password hashing.
	const int client_pwhash_memlimit = 128 << 20; // Likewise, memory cost.

	std::string dumpAllGroups(const std::optional<Client>& client) const {
		std::cout << "Dumping from ordered_groups_vec: ";

		boost::json::object groups_json;
		boost::json::array group_heirarchy_json;
		int group_editable_threshold;
		if (this->clientHasPermission(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS)))
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
	std::string dumpAllUsers(const std::optional<FuzeHttp::Client>& client) const {
		boost::json::object users_json;
		int client_rank = this->getClientRank(client);
		bool client_has_manage_permissions_permission = this->clientHasPermission(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS));
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

	const std::filesystem::path& getMediaLocation() const { return media_location; }
	std::unordered_map<int, std::shared_ptr<TestObject>> objects;
	std::unordered_map<int, std::shared_ptr<TestChildObject>> child_objects;
	// const std::filesystem::path& getProgramLocation() const { return program_location; }
private:
	// const std::filesystem::path program_location;
};
} // namespace Example
} // export namespace FuzeHttp
