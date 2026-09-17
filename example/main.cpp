// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
// Fuze Mediaboard was built on top of an example project by Vinnie Falco.
// https://github.com/vinniefalco/CppCon2018

// #include "FuzeHttpUtils.hpp"
// #include "FuzeHttpServer.hpp"
// #include "PermissionObject.hpp"
#include "shared_state.hpp"
#include <cstdlib>
#include <print>
#include <string>
#include <vector>
// import FuzeHttp.Example.Migrations;
import FuzeHttp.PermissionObject;
import FuzeHttp.Server;
import FuzeHttp.Utils;
import FuzeHttp.Example.Urls;
import FuzeDBI;

const std::string current_version = "0.1.4";
#ifdef PROJECT_FOLDER
const std::string project_name = PROJECT_FOLDER; // used for folder name
#else
const std::string project_name = "FuzeHttp_Project";
#endif
using namespace FuzeHttp;

std::vector<FuzeHttp::TemplateMacro*> template_macros{
	new TemplateOption<std::string>("favicon_url", "https://fuze.page/favicon.ico"),
	new TemplateConstant("group_max_name", static_cast<int>(Group::MAX_NAME)),
	new TemplateConstant("account_max_username", static_cast<int>(Account::MAX_USERNAME)),
	new TemplateConstant("mediaboard_version", current_version)
};

// struct TemplateOptionsStruct {
// 	std::string site_name;
// 	std::string favicon_url;
// 	int thumbnail_size;
// } template_options_struct;

int main(int argc, char* argv[]) {
	// runtime config
	StateConfig state_config;
	template_macros.push_back(new TemplateOptionPtr("site_name", &state_config.server_name, {.default_value=std::string("FuzeHttp Example")}));

	std::println("Initialising server...");
	// shared_state state(state_config);
	// state.start();

	FuzeHttp::Server<shared_state> server(current_version);
	std::println("Finished Initialising server...");
	if (int return_code; (return_code = server.processOptions(argc, argv, template_macros, project_name)) != -1)
		return return_code;
	std::println("Finished processing options... adding confuig...");
	server.state->config = state_config;
	std::println("Finished adding config... adding URLs...");
	addURLsToController(&server.controller);
	std::println("Running server...");
	server.run();

	return EXIT_SUCCESS;
}
