// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
// FuzeHttp was built on top of an example project by Vinnie Falco.
// https://github.com/vinniefalco/CppCon2018

#include <cstdlib>
#include <print>
#include <string>
#include <vector>
import FuzeHttp.Example.Config;
import FuzeHttp.Example.State;
import FuzeHttp.PermissionObject;
import FuzeHttp.Server;
import FuzeHttp.ProgramOptions;
import FuzeHttp.Utils;
import FuzeHttp.Example.Urls;
import FuzeDBI;

const std::string current_version = "0.1.4";
#ifdef PROJECT_FOLDER
const std::string project_name = PROJECT_FOLDER; // used for folder name
#else
const std::string project_name = "FuzeHttp_Project";
#endif
using namespace FuzeHttp::Example;

// struct ProgramOptionsStruct {
// 	std::string site_name;
// 	std::string favicon_url;
// 	int thumbnail_size;
// } template_options_struct;

int main(int argc, char* argv[]) {
	// runtime config
	FuzeHttp::ProgramOptions server_options;
	StateConfig state_config;
	addProgramOptions(&server_options, &state_config);

	std::println("Initialising server...");

	FuzeHttp::Server<shared_state> server(current_version);
	std::println("Finished Initialising server...");
	if (int return_code; (return_code = server.processOptions(argc, argv, std::move(server_options), project_name)) != -1)
		return return_code;
	std::println("Finished processing options... adding confuig...");
	server.state->config = state_config;
	std::println("Finished adding config... adding URLs...");
	addURLsToController(&server.controller);
	std::println("Running server...");
	server.run();

	return EXIT_SUCCESS;
}
