// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include "Controller.hpp"
export module FuzeHttp.Example.Urls;
import FuzeHttp.Example.State;
import FuzeHttp.Example.Views;
import FuzeHttp.Example.Views_registration;
// #include "views.hpp"
// #include "views_registration.hpp"
import FuzeHttp.State;
import FuzeHttp.Example.Resolvers;

using namespace FuzeHttp;
using namespace FuzeHttp::Example;
using namespace http;

export namespace FuzeHttp::Example {
void addURLsToController(FuzeHttp::Controller<shared_state*>* controller) {
	// C-style strings are immutable parts of the URL, and strings/ints are variables passed into the view.
	// Client{} is used when the function needs to identify the user via a cookie.
	controller->addPattern(verb::get, showMainPage,						"*");
	controller->addPattern(verb::post, createGroup,						"api", "create_group"); // TODO move to server/permissions
	controller->addPattern(verb::delete_, deleteGroup,					"api", "group", int());
	controller->addPattern(verb::delete_, removeMemberFromGroup,		"api", "group", int(), "member", int());
	controller->addPattern(verb::get, getGroupMembers,					"api", "group", int(), "members");
	controller->addPattern(verb::get, getGroups,						"api", "groups");
	controller->addPattern(verb::get, getTestObject, 					"test", ObjectResolver{});
	controller->addPatterns()
		(verb::get, getChildObject, 					"test", ObjectResolver{}, ChildObjectResolver{})
		(verb::put, setGroupHeirarchy,					"api", "group_heirarchy")
		(verb::get, getServerPermissions,				"api", "server", "permissions")
		(verb::post, addServerGroupPermission,			"api", "server", "permissions", "group", int())
		(verb::put, updateServerGroupPermissions,		"api", "server", "permissions", "group", int())
		(verb::delete_, deleteServerGroupPermission,	"api", "server", "permissions", "group", int())
		(verb::post, addServerUserPermission,			"api", "server", "permissions", "user", int())
		(verb::put, updateServerUserPermissions,		"api", "server", "permissions", "user", int())
		(verb::delete_, deleteServerUserPermission,		"api", "server", "permissions", "user", int())
		(verb::post, addGroupsToUser,					"api", "user", int(), "add_groups") // TODO move to server/permissions
		(verb::get, client, 							"api", "user", "client")
		(verb::get, getUsers,							"api", "users")
		(verb::get, acceptInvite,						"invite", std::string())

		(verb::post, requestNewAccountParameters,		"registration", "request_new_account_parameters")
		(verb::post, createNewAccount,					"registration", "create_new_account")
		(verb::post, requestLoginParameters, 			"registration", "request_login_parameters")
		(verb::post, login, 							"registration", "login")
		(verb::post, logout,							"registration", "logout")
		(verb::post, changePassword, 					"registration", "change_password");
}
} //export namespace FuzeHttp::Example
