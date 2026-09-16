// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
#include "urls.hpp"
#include "shared_state.hpp"
import FuzeHttp.Example.Views;
import FuzeHttp.Example.Views_registration;
// #include "views.hpp"
// #include "views_registration.hpp"
import FuzeHttp.State;
import FuzeHttp.Example.Resolvers;

using namespace FuzeHttp;
using namespace FuzeHttp::Example;
using namespace http;

namespace FuzeHttp {
template<>
void addURLsToController<shared_state>(FuzeHttp::Controller<shared_state*>* controller) {
	// C-style strings are immutable parts of the URL, and strings/ints are variables passed into the view.
	// Client{} is used when the function needs to identify the user via a cookie.
	controller->addPattern(verb::get, showMainPage,						"*");
	controller->addPattern(verb::post, createGroup,						"api", "create_group"); // TODO move to server/permissions
	controller->addPattern(verb::delete_, deleteGroup,					"api", "group", int());
	controller->addPattern(verb::delete_, removeMemberFromGroup,		"api", "group", int(), "member", int());
	controller->addPattern(verb::get, getGroupMembers,					"api", "group", int(), "members");
	controller->addPattern(verb::get, getGroups,						"api", "groups");
	controller->addPattern(verb::get, getTestObject, 					"test", ObjectResolver{});
	controller->addPattern(verb::get, getChildObject, 					"test", ObjectResolver{}, ChildObjectResolver{});
	controller->addPattern(verb::put, setGroupHeirarchy,				"api", "group_heirarchy");
	controller->addPattern(verb::get, getServerPermissions,				"api", "server", "permissions");
	controller->addPattern(verb::post, addServerGroupPermission,		"api", "server", "permissions", "group", int());
	controller->addPattern(verb::put, updateServerGroupPermissions,		"api", "server", "permissions", "group", int());
	controller->addPattern(verb::delete_, deleteServerGroupPermission,	"api", "server", "permissions", "group", int());
	controller->addPattern(verb::post, addServerUserPermission,			"api", "server", "permissions", "user", int());
	controller->addPattern(verb::put, updateServerUserPermissions,		"api", "server", "permissions", "user", int());
	controller->addPattern(verb::delete_, deleteServerUserPermission,	"api", "server", "permissions", "user", int());
	controller->addPattern(verb::post, addGroupsToUser,					"api", "user", int(), "add_groups"); // TODO move to server/permissions
	controller->addPattern(verb::get, client, 							"api", "user", "client");
	controller->addPattern(verb::get, getUsers,							"api", "users");
	controller->addPattern(verb::get, acceptInvite,						"invite", std::string());

	controller->addPattern(verb::post, requestNewAccountParameters, 	"registration", "request_new_account_parameters");
	controller->addPattern(verb::post, createNewAccount,  				"registration", "create_new_account");
	controller->addPattern(verb::post, requestLoginParameters,  		"registration", "request_login_parameters");
	controller->addPattern(verb::post, login, 							"registration", "login");
	controller->addPattern(verb::post, logout,							"registration", "logout");
	controller->addPattern(verb::post, changePassword,  				"registration", "change_password");
}
}
