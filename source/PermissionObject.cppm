module;
// #include "Group.hpp"
#include <boost/json/object.hpp>
#include <iostream>
#include <print>
#include <unordered_set>
#include <vector>
export module FuzeHttp.PermissionObject;

export import FuzeHttp.Group;
export import FuzeHttp.PermissionCollection;
import FuzeDBI;

export namespace FuzeHttp {
enum class BUILTIN_GROUPS {
	OWNER = 0,
	USERS = 1,
	PUBLIC = 2
};

struct Account {
	inline static const int PUBLIC = 0;
	inline static const size_t MAX_USERNAME = 32;
	inline static const size_t MIN_PASSWORD = 3;
	const int id;
	std::optional<int> client_id;
	std::string username;
};

struct Client {
	int id;
	std::optional<int> account_id;
	// const std::string session_id;
};

class PermissionObjectBase {
	friend class PermissionManagedObject;
	friend class PermissionManager;
public:
	PermissionObjectBase(int permission_object_id, FuzeDBI::Connection* fuze_dbi) // On extraction from database
			: id(permission_object_id),
			fuze_dbi(fuze_dbi) {
		this->cacheAllPermissions();
	} // retrieve from database
	PermissionObjectBase(FuzeDBI::Connection* fuze_dbi) // On new object creation
			: fuze_dbi(fuze_dbi),
			id(fuze_dbi->query<int>("SELECT permission_object_id FROM _sequences")) {
		fuze_dbi->query<void>("UPDATE _sequences SET permission_object_id = $1", this->id+1);
	}
	void cacheAllPermissions() {
		std::cout << "[PermissionObjectBase] retrieving permissions for " << this->id << ": ";
		for (auto permission_collection_tuple : fuze_dbi->queryRows<std::tuple<int, std::optional<int>, std::optional<int>>>("SELECT id, account_id, permission_group_id FROM permission_collection WHERE permission_object_id = $1", this->id)) {
			std::optional<int> account_id = std::get<1>(permission_collection_tuple);
			std::optional<int> group_id = std::get<2>(permission_collection_tuple);
			PermissionCollection permission_collection(std::get<0>(permission_collection_tuple), account_id, group_id);

			for (auto permission_setting_tuple : fuze_dbi->queryRows<std::tuple<int, int, int>>("SELECT id, permission_number, setting FROM permission_setting WHERE permission_collection_id = $1", permission_collection.getId())) {
				std::cout << std::get<0>(permission_setting_tuple) << ", ";
				permission_collection.addPermissionSetting(std::get<0>(permission_setting_tuple), static_cast<PERMISSION>(std::get<1>(permission_setting_tuple)), static_cast<THREE_STATE_SETTING>(std::get<2>(permission_setting_tuple)));
			}
			if (permission_collection.getAccountOrGroupEnumValue() == ACCOUNT_OR_GROUP::ACCOUNT)
				this->account_permissions.emplace(account_id.value(), permission_collection);
			else
				this->group_permissions.emplace(group_id.value(), permission_collection);
		}
		std::cout << "done." << std::endl;
	}
	void addGroupPermissionCollection(int group_id) {
		if (this->permissionCollectionExistsForGroup(group_id))
			throw std::runtime_error(std::format("Attempted to create duplicate permission collection for group {}", group_id));
		int new_permission_collection_id = fuze_dbi->query<int>("SELECT permission_collection_id FROM _sequences");
		fuze_dbi->query<void>("UPDATE _sequences SET permission_collection_id = $1", new_permission_collection_id+1);
		fuze_dbi->query<void>("INSERT INTO permission_collection(id, permission_object_id, permission_group_id) VALUES ($1, $2, $3)", new_permission_collection_id, this->id, group_id);
		std::cout << "[PermissionObjectBase] adding group permission_collection for group " << group_id << std::endl;
		PermissionCollection permission_collection(new_permission_collection_id, {}, group_id);
		this->group_permissions.emplace(group_id, permission_collection);
	}
	void addAccountPermissionCollection(int account_id) {
		if (this->permissionCollectionExistsForAccount(account_id))
			throw std::runtime_error(std::format("Attempted to create duplicate permission collection for account {}", account_id));
		int new_permission_collection_id = fuze_dbi->query<int>("SELECT permission_collection_id FROM _sequences");
		fuze_dbi->query<void>("UPDATE _sequences SET permission_collection_id = $1", new_permission_collection_id+1);
		fuze_dbi->query<void>("INSERT INTO permission_collection(id, permission_object_id, account_id) VALUES ($1, $2, $3)", new_permission_collection_id, this->id, account_id);
		std::cout << "[PermissionObjectBase] adding account permission_collection for account " << account_id << std::endl;
		PermissionCollection permission_collection(new_permission_collection_id, account_id, {});
		this->account_permissions.emplace(account_id, permission_collection);
	}
	void removeGroupPermissionCollection(int group_id) {
		auto it = this->group_permissions.find(group_id);
		if (it == this->group_permissions.end())
			throw std::runtime_error(std::format("Attempted to delete group {} which doesn't exist", group_id));
		fuze_dbi->query<void>("DELETE FROM permission_collection WHERE permission_group_id = $1", group_id);
		this->group_permissions.erase(it);
	}
	void removeAccountPermissionCollection(int account_id) {
		if (!this->account_permissions.contains(account_id))
			throw std::runtime_error(std::format("Attempted to delete account {} which doesn't exist", account_id));
		fuze_dbi->query<void>("DELETE FROM permission_collection WHERE account_id = $1", account_id);
		this->account_permissions.erase(account_id);
	}
	void setGroupPermission(int group_id, PERMISSION permission_type, THREE_STATE_SETTING setting) {
		if (!this->group_permissions.contains(group_id))
			this->addGroupPermissionCollection(group_id);
		this->group_permissions.at(group_id).setPermission(permission_type, setting, fuze_dbi);
	}
	void setAccountPermission(int user_id, PERMISSION permission_type, THREE_STATE_SETTING setting) {
		if (auto it = this->account_permissions.find(user_id); it == this->account_permissions.end())
			this->addAccountPermissionCollection(user_id);
		this->account_permissions.at(user_id).setPermission(permission_type, setting, fuze_dbi);
	}
	bool passPermissionForGroup(bool inherited_permission, PERMISSION permission, int group_id) const {
		inherited_permission = this->passInheritedPermissionForGroup(inherited_permission, permission, group_id);
		// Check if a group permission is set for this object
		std::unordered_map<int, PermissionCollection>::const_iterator group_iterator = this->group_permissions.find(group_id);
		if (group_iterator != this->group_permissions.end())
			inherited_permission = group_iterator->second.passPermission(permission, inherited_permission);
		return inherited_permission;
	}
	bool passPermissionForAccount(bool inherited_permission, PERMISSION permission, int account_id) const {
		inherited_permission = this->passInheritedPermissionForAccount(inherited_permission, permission, account_id);
		if (auto it = this->account_permissions.find(account_id); it != this->account_permissions.end()) // Check if a client permission is set for this object
			inherited_permission = it->second.passPermission(permission, inherited_permission);
		return inherited_permission;
	}
	virtual const std::vector<int>* getOrderedGroups() const = 0;
	virtual std::vector<int> getOrderedGroupsContainingMember(int user_id) const = 0;
	virtual bool passInheritedPermissionForGroup(bool inherited_permission, PERMISSION permission, int group_id) const = 0;
	virtual bool passInheritedPermissionForAccount(bool inherited_permission, PERMISSION permission, int account_id) const = 0;
	virtual int getClientRank(const std::optional<Client>& client) const = 0;
	virtual int getGroupRank(int group_id) const = 0;
	virtual int getAccountRank(int account_id) const = 0;
	virtual const std::unordered_map<int, Account>& getAccounts() const = 0;
	bool clientHasPermission(const std::optional<Client>& client, PERMISSION permission) const {
		bool inherited_permission = false;
		// PUBLIC and USERS are built-in, that is, they are never placed in an account's group list. This is because every account is implicitly a part of these two groups
		inherited_permission = this->passPermissionForGroup(inherited_permission, permission, static_cast<int>(BUILTIN_GROUPS::PUBLIC));
		if (client && client.value().account_id) {
			std::cout << "[clientHasPermission] account_id: " << client.value().account_id.value() << std::endl;
			inherited_permission = this->passPermissionForGroup(inherited_permission, permission, static_cast<int>(BUILTIN_GROUPS::USERS));
			std::vector<int> user_ordered_groups = this->getOrderedGroupsContainingMember(client.value().account_id.value());
			for (std::vector<int>::const_reverse_iterator it = user_ordered_groups.rbegin(); it != user_ordered_groups.rend(); it++) {
				inherited_permission = this->passPermissionForGroup(inherited_permission, permission, *it);
			}
			inherited_permission = this->passPermissionForAccount(inherited_permission, permission, client.value().account_id.value());
		}
		return inherited_permission;
	}
	bool clientHasPermissionForGroup(const std::optional<Client>& client, PERMISSION permission, int group_id) const {
		if (!this->clientHasPermission(client, permission))
			return false;
		return this->getClientRank(client) < this->getGroupRank(group_id);
	}
	bool clientHasPermissionForAccount(const std::optional<Client>& client, PERMISSION permission, int account_id) const {
		if (!this->clientHasPermission(client, permission))
			return false;
		return this->getClientRank(client) < this->getAccountRank(account_id);
	}
	bool permissionCollectionExistsForGroup(int group_id) const { return this->group_permissions.contains(group_id); }
	bool permissionCollectionExistsForAccount(int account_id) const { return this->account_permissions.contains(account_id); }
	boost::json::object getPermissionCollectionsAsJson() const {
		// client_rank not used because client_editable status is given by dumpAllGroups()/dumpAllUsers()
		// int client_rank = this->getUserRank(client_id);

		boost::json::object group_permissions_json;
		for (int i = 0; i < this->getOrderedGroups()->size(); i++) {
			// for (int group_id : *(this->getOrderedGroups())) {
			int group_id = (*(this->getOrderedGroups()))[i];
			std::unordered_map<int, PermissionCollection>::const_iterator group_permission_collection_it = this->group_permissions.find(group_id);
			if (group_permission_collection_it != this->group_permissions.end()) {
				const std::unordered_map<PERMISSION, PermissionSetting>* permission_settings = group_permission_collection_it->second.getPermissionMap();
				boost::json::object permission_collection_json;
				for (std::unordered_map<PERMISSION, PermissionSetting>::const_iterator permission_it = permission_settings->begin(); permission_it != permission_settings->end(); permission_it++) {
					permission_collection_json[std::to_string(static_cast<int>(permission_it->first))] = static_cast<int>(permission_it->second.get());
				}
				// bool group_permission_is_client_editable;
				// if (this->userHasPermission(client_id, PERMISSION::MANAGE_PERMISSIONS) && client_rank < i)
				// 	group_permission_is_client_editable	= true;
				// else
				// 	group_permission_is_client_editable = false;
				// group_permissions_json[std::to_string(group_id)]["client_editable"] = group_permission_is_client_editable;
				group_permissions_json.emplace(std::to_string(group_id), boost::json::value{{"permission_collection", permission_collection_json}});
			}
		}

		boost::json::object user_permissions_json;
		// int user_rank = this->getUserRank(client_id);
		std::cout << "[PermissionManager] getting user_permissions_json..." << std::endl;
		// for (const std::pair<int, User> user : *this->getUsers()) {
		//const std::unordered_map<int, Account>& _accounts = this->getAccounts();
		//for (std::unordered_map<int, Account>::const_iterator user_it = _accounts.begin(); user_it != _accounts.end(); user_it++) {
		for (const std::pair<int, Account>& account_pair : this->getAccounts()) {
			int account_id = account_pair.first;
			std::cout << account_id << ", ";
			std::unordered_map<int, PermissionCollection>::const_iterator user_permission_collection_it = this->account_permissions.find(account_id);
			if (user_permission_collection_it != this->account_permissions.end()) {
				const std::unordered_map<PERMISSION, PermissionSetting>* permission_settings = user_permission_collection_it->second.getPermissionMap();
				boost::json::object permission_collection_json;
				for (std::unordered_map<PERMISSION, PermissionSetting>::const_iterator permission_it = permission_settings->begin(); permission_it != permission_settings->end(); permission_it++) {
					permission_collection_json[std::to_string(static_cast<int>(permission_it->first))] = static_cast<int>(permission_it->second.get());
				}
				user_permissions_json.emplace(std::to_string(account_id), boost::json::value{{"permission_collection", permission_collection_json}});
			}
		}
		std::cout << "done." << std::endl;

		return {
			{"group_permissions", group_permissions_json},
			{"user_permissions", user_permissions_json}
		};
	}
protected:
	int getPermissionObjectId() const { return this->id; }
private:
	int id;
	FuzeDBI::Connection* fuze_dbi;
	std::unordered_map<int, PermissionCollection> group_permissions;
	std::unordered_map<int, PermissionCollection> account_permissions;
};

class PermissionManager : public PermissionObjectBase {
public:
	PermissionManager(int permission_object_id, FuzeDBI::Connection* fuze_dbi)
			: PermissionObjectBase(0, fuze_dbi) {
		this->cacheAllGroups();
		this->cacheAllAccounts();
		this->grantOwnerPrivileges();
	}
	const std::vector<int>* getOrderedGroups() const override {
		return &(this->ordered_groups);
	}
	std::vector<int> getOrderedGroupsContainingMember(int user_id) const override {
		std::vector<int> ordered_groups_containing_member;
		ordered_groups_containing_member.reserve(this->ordered_groups.size());
		for (int group_id : this->ordered_groups) {
			if (this->groups.at(group_id).containsMember(user_id))
				ordered_groups_containing_member.push_back(group_id);
		}
		return ordered_groups_containing_member;
	}
	bool groupExists(int group_id) const {
		std::unordered_map<int, Group>::const_iterator it = this->groups.find(group_id); 
		return it != this->groups.end();
	}
	int getClientRank(const std::optional<Client>& client) const override {
		if (!client || !client.value().account_id)
			return this->ordered_groups.size(); // This is the least privileged rank
		else
			return this->getAccountRank(client.value().account_id.value());
	}
	int getAccountRank(int account_id) const override {
		// if (this->owner_id && account_id == this->owner_id.value())
		// 	return 0; // This is the most privileged rank
		int i;
		for (i = 0; i < this->ordered_groups.size() - 2; i++) { // 2 is subtracted because USERS and PUBLIC are hard-coded groups
			if (this->groups.at(ordered_groups[i]).containsMember(account_id))
				break;
		}
		return i;
	}
	std::optional<int> getIdFromUsername(const std::string& username) const {
		if (auto it = this->username_to_id_map.find(username); it != this->username_to_id_map.end())
			return it->second;
		else
			return {};
	}
	std::string getUsernameFromAccount(int account_id) const {
		return this->accounts.at(account_id).username;
	}
	bool userExists(std::string username) const {
		std::unordered_map<std::string, int>::const_iterator it = this->username_to_id_map.find(username);
	   	return it != this->username_to_id_map.end();
	};
	bool accountMatchesPassword(int account_id, const std::string& password) {
		for (int id :fuze_dbi->queryRows<int>("SELECT id FROM account WHERE password_hash_hash_base64 = $1", password))
			return true;
		return false;
	}
	void changeAccountPassword(int account_id, const char* password_hash_hash_base64, const char* intermediate_salt_base64) {
		fuze_dbi->query<void>("UPDATE account SET password_hash_hash_base64 = $1, intermediate_salt_base64 = $2 WHERE id = $3", password_hash_hash_base64, intermediate_salt_base64, account_id);
	}
	// bool checkUserKey(int user_id, std::string key) const {
	// 	return this->users.at(user_id).keyMatches(key);
	// }
	const std::unordered_map<int, Account>& getAccounts() const override {
		return this->accounts;
	}
	int getGroupRank(int group_id) const override {
		int rank;
		for (rank = 0; this->ordered_groups[rank] != group_id; rank++)
			;
		return rank + 1; // 1 is added because the ADMINISTRATORS group is one rank below OWNER
	}
	void eraseGroup(int group_id) {
		std::vector<int>::const_iterator it = std::find(this->ordered_groups.begin(), this->ordered_groups.end(), group_id);
		std::cout << *it << " should match " << group_id << std::endl;
		for (int member_id : this->groups.at(group_id).getMembers()) {
			this->removeUserFromGroup(member_id, group_id);
		}
		for (int permission_collection_id : fuze_dbi->queryRows<int>("SELECT id FROM permission_collection WHERE permission_group_id = $1", group_id))
			fuze_dbi->query<void>("DELETE FROM permission_setting WHERE permission_collection_id = $1", permission_collection_id);
		fuze_dbi->query<void>("DELETE FROM permission_collection WHERE permission_group_id = $1", group_id);
		fuze_dbi->query<void>("DELETE FROM permission_group WHERE id = $1", group_id);
		this->ordered_groups.erase(it);
		this->groups.erase(group_id);
		this->saveGroupHeirarchy();
	}
	boost::json::array getGroupMembersAsJson(int group_id) const {
		boost::json::array members;
		for (int member_id : this->groups.at(group_id).getMembers())
			members.emplace_back(member_id);
		return members;
	}
	void removeUserFromGroup(int account_id, int group_id) {
		this->groups.at(group_id).removeMember(account_id);
		fuze_dbi->query<void>("DELETE FROM permission_group_account WHERE permission_group_id = $1 AND account_id = $2", group_id, account_id);
	}
	int addGroup(std::string group_name, int group_rank){
		if (group_name.length() > Group::MAX_NAME)
			throw std::runtime_error(std::format("Group name length {} is over the limit of {}", group_name.length(), Group::MAX_NAME));
		int new_group_id = fuze_dbi->query<int>("SELECT permission_group_id FROM _sequences");
		fuze_dbi->query<void>("UPDATE _sequences SET permission_group_id = $1", new_group_id+1);
		fuze_dbi->query<void>("INSERT INTO permission_group(id, name) VALUES ($1, $2)", new_group_id, group_name.c_str());
		Group new_group(new_group_id, group_name);
		this->groups.emplace(new_group_id, new_group);
		// https://stackoverflow.com/a/6935419
		std::vector<int>::const_iterator group_to_insert_above = this->ordered_groups.begin() + group_rank;
		this->ordered_groups.insert(group_to_insert_above, new_group_id);
		this->saveGroupHeirarchy();
		return new_group_id;
	}
	void addAccountToGroup(int account_id, int group_id) {
		if (!this->groupExists(group_id))
			throw std::runtime_error(std::format("[addAccountToGroup] Group {} does not exist", group_id));
		if (!this->accountExists(account_id))
			throw std::runtime_error(std::format("[addAccountToGroup] Account {} does not exist", account_id));
		if (static_cast<BUILTIN_GROUPS>(group_id) == BUILTIN_GROUPS::USERS || static_cast<BUILTIN_GROUPS>(group_id) == BUILTIN_GROUPS::PUBLIC)
			throw std::runtime_error("[addAccountToGroup] Attempted to add user to one or more groups to which no user can be added, namely, the \"USERS\" and \"PUBLIC\" groups.");
		if (!this->groups.at(group_id).containsMember(account_id)) {
			this->groups.at(group_id).addMember(account_id);
			fuze_dbi->query<void>("INSERT INTO permission_group_account(permission_group_id, account_id) VALUES ($1, $2)", group_id, account_id);
		}
	}
	void setOrderedGroups(std::vector<int> ordered_groups) {
		this->ordered_groups = ordered_groups;
		this->saveGroupHeirarchy(); // Apply changes to the database
	}
	int createAccount(const std::string& username, const char* password_hash_hash, const char* intermediate_salt_base64) {
		if (this->accountExists(username))
			throw std::runtime_error("An account with this username already exists");
		int new_account_id = fuze_dbi->query<int>("SELECT account_id FROM _sequences");
		fuze_dbi->query<void>("UPDATE _sequences SET account_id = $1", new_account_id+1);
		fuze_dbi->query<void>("INSERT INTO account(id, username, password_hash_hash_base64, intermediate_salt_base64) VALUES ($1, $2, $3, $4)", new_account_id, username.c_str(), password_hash_hash, intermediate_salt_base64);
		this->accounts.emplace(new_account_id, Account{
			.id = new_account_id,
			.username = username
		});
		this->username_to_id_map.emplace(username, new_account_id);
		return new_account_id;
	}
	bool accountExists(const std::string username) const { return this->username_to_id_map.contains(username); }
	bool accountExists(int account_id) const { return this->accounts.contains(account_id); }
	bool ownerExists() const {
		if (!this->groups.contains(static_cast<int>(BUILTIN_GROUPS::OWNER)))
			throw std::runtime_error("[PermissionManager] Group OWNER does not exist");
		const Group* owner_group = this->getGroup(static_cast<int>(BUILTIN_GROUPS::OWNER));
		if (owner_group->getMembers().empty())
			return false;
		else if (owner_group->getMembers().size() > 1)
			throw std::runtime_error("[PermissionManager] Multiple owners detected. Mediaboard can only have one owner.");
		else
			return true;
	}
	// PermissionManager is the highest level, so there is no parent to inherit from
	bool passInheritedPermissionForGroup(bool inherited_permission, PERMISSION permission, int group_id) const override { return inherited_permission; }
	bool passInheritedPermissionForAccount( bool inherited_permission, PERMISSION permission, int account_id ) const override { return inherited_permission; }
	const std::optional<int> owner_id;
protected:
	const Group* getGroup(int group_id) const {
		return &(this->groups.at(group_id));
	}
	// const Account* getAccount(std::string username) const {
	// 	return &(this->accounts.at(this->username_to_id_map.at(username)));
	// }
	// bool checkUserPassword(int user_id, std::string password) const {
	// 	return this->users.at(user_id).passwordMatches(password);
	// }
	// std::string getUserKey(int user_id) const {
	// 	return this->users.at(user_id).getKey();
	// }
	// const User* createUser(std::string username, std::string password) {
	// 	User new_user(username, password);
	// 	this->users.emplace(new_user.getId(), new_user);
	// 	this->username_to_id_map.emplace(username, new_user.getId());
	// 	// Every registered account is implicitly a member of the "Users" group
	// 	// this->groups.at(static_cast<int>(BUILTIN_GROUPS::USERS)).addMember(new_user.getId());
	// 	return &(this->users.at(new_user.getId()));
	// }
	void cacheAllGroups() {
		std::cout << "[PermissionManager] Retreiving groups from database... ";
		// struct db_group_array* group_array = db_retrieve_groups();
		for (auto group_tuple : fuze_dbi->queryRows<std::tuple<int, std::string>>("SELECT id, name FROM permission_group")) {
			Group group(std::get<0>(group_tuple), std::get<1>(group_tuple));
			this->groups.emplace(std::get<0>(group_tuple), group);
		}
		std::cout << "added " << this->groups.size() << " groups";

		for (auto group_id : fuze_dbi->queryRows<int>("SELECT permission_group_id FROM permission_group_heirarchy ORDER BY rank")) {
			this->ordered_groups.push_back(group_id);
		}
		std::cout << ", established heirarchy";

		for (auto group_member : fuze_dbi->queryRows<std::tuple<int, int>>("SELECT permission_group_id, account_id FROM permission_group_account")) {
			this->groups.at(std::get<0>(group_member)).addMember(std::get<1>(group_member));
		}
		std::cout << ", added users to groups." << std::endl;

		// Check that the permission_group table is consistent with the permission_group_heirarchy table
		std::cout << "[PermissionManager] Checking consistency between groups and heirarchy..." << std::endl;
		bool consistency_test_passed = true;
		if (this->ordered_groups.size() != this->groups.size()) {
			std::cerr << std::string("Number of groups does not match") << std::endl;
			consistency_test_passed = false;
		}
		std::unordered_set<int> new_group_order_set;
		for (int group_id : this->ordered_groups) {
			// Check for duplicates
			std::unordered_set<int>::const_iterator duplicate_check_it = new_group_order_set.find(group_id);
			if (duplicate_check_it != new_group_order_set.end()) {
				std::cerr << std::format("Duplicate group {} detected.", group_id) << std::endl;
				consistency_test_passed = false;
			}
			else
				new_group_order_set.insert(group_id);

			// Check if all groups exist
			if (!this->groupExists(group_id)) {
				std::cerr << std::format("Group {} does not exist.", group_id) << std::endl;
				consistency_test_passed = false;
			}
		}
		if (consistency_test_passed)
			std::cout << "[PermissionManager] No issues were found." << std::endl;
		else
			throw std::runtime_error("[PermissionManager] Test failed. Check the permission_group and permission_group_heirarchy tables in the database.");
	}
	void cacheAllAccounts()  {
		std::cout << "[PermissionManager] Retreiving accounts from database... ";
		for (auto user_t : fuze_dbi->queryRows<std::tuple<int, std::string>>("SELECT id, username FROM account")) {
			std::cout << std::get<0>(user_t) << ", ";
			this->accounts.emplace(std::get<0>(user_t), Account{
				.id = std::get<0>(user_t),
				.username = std::get<1>(user_t)
			});
			this->username_to_id_map.emplace(std::get<1>(user_t), std::get<0>(user_t));
		}
		std::cout << "done." << std::endl;
	}

	std::unordered_map<int, Account> accounts;
private:
	void grantOwnerPrivileges()  {
		std::cout << "[PermissionManager] grantOwnerPrivileges()" << std::endl;
		for (int permission_number = 0; permission_number < static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS); permission_number++) {
			if (!this->passPermissionForGroup(false, static_cast<PERMISSION>(permission_number), static_cast<int>(BUILTIN_GROUPS::OWNER)))
				this->setGroupPermission(static_cast<int>(BUILTIN_GROUPS::OWNER), static_cast<PERMISSION>(permission_number), THREE_STATE_SETTING::ALLOW);
		}
	}
	void saveGroupHeirarchy() const {
		std::cout << "[PermissionManager] Saving new group heirarchy: ";
		fuze_dbi->query<void>("DELETE FROM permission_group_heirarchy");
		for (int rank = 0; rank < this->ordered_groups.size(); rank++) {
			fuze_dbi->query<void>("INSERT INTO permission_group_heirarchy(rank, permission_group_id) VALUES ($1, $2)", rank, this->ordered_groups[rank]);
			std::cout << rank << ": " << this->ordered_groups[rank] << ", ";
		}
		std::cout << "done." << std::endl;
	}

	std::unordered_map<std::string, int> username_to_id_map;
	std::unordered_map<int, Group> groups;
	std::vector<int> ordered_groups;
};

class PermissionManagedObject : public PermissionObjectBase {
public:
	// Existing object
	PermissionManagedObject(PermissionObjectBase* parent_object, int permission_object_id, FuzeDBI::Connection* fuze_dbi)
			: PermissionObjectBase(permission_object_id, fuze_dbi), parent_object(parent_object) {
	}
	// New object
	PermissionManagedObject(PermissionObjectBase* parent_object, FuzeDBI::Connection* fuze_dbi)
			: PermissionObjectBase(fuze_dbi), parent_object(parent_object) {
	}
	bool isOwnedBy(const Client& client) const;
	const std::vector<int>* getOrderedGroups() const override {
		return this->parent_object->getOrderedGroups();
	}
	std::vector<int> getOrderedGroupsContainingMember(int user_id) const override {
		return this->parent_object->getOrderedGroupsContainingMember(user_id);
	}
	int getClientRank(const std::optional<Client>& client) const override {
		return this->parent_object->getClientRank(client);
	}
	int getAccountRank(int account_id) const override {
		return this->parent_object->getAccountRank(account_id);
	}
	int getGroupRank(int group_id) const override {
		return this->parent_object->getGroupRank(group_id);
	}
	const std::unordered_map<int, Account>& getAccounts() const override {
		return this->parent_object->getAccounts();
	}
	bool passInheritedPermissionForGroup(bool inherited_permission, PERMISSION permission, int group_id) const override {
		return this->parent_object->passPermissionForGroup(inherited_permission, permission, group_id);
	}
	bool passInheritedPermissionForAccount(bool inherited_permission, PERMISSION permission, int account_id) const override {
		return this->parent_object->passPermissionForAccount(inherited_permission, permission, account_id);
	}
private:
	// std::optional<int> owner_account_id;
	PermissionObjectBase* parent_object;
};
} // namespace FuzeHttp
