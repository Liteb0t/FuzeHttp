// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
// Fuze Mediaboard was built on top of an example project by Vinnie Falco.
// https://github.com/vinniefalco/CppCon2018

#include "FuzeHttpUtils.hpp"
#include "FuzeHttpServer.hpp"
#include "PermissionObject.hpp"
#include "shared_state.hpp"
#include <boost/asio/signal_set.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#define BOOST_DLL_USE_STD_FS
#include <boost/dll.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/hash2/md5.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/smart_ptr/make_shared_array.hpp>
#ifdef WITH_MAGICK
#include <Magick++.h>
#endif
#include <cstdlib>
#include <iostream>
#include <print>
#include <string>
#include <vector>

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
	StateConfig state_config;	// Macros which link to state_config
	template_macros.push_back(new TemplateOptionPtr("site_name", &state_config.server_name, {.default_value=std::string("FuzeHttp Example")}));
	FuzeHttp::Server server(current_version);
	if (int return_code; (return_code = server.processOptions(argc, argv, template_macros, project_name)) != -1)
		return return_code;
	std::cout << "Initialising shared state..." << std::endl;
	shared_state* state;
	try {
		bool create_owner_account = server.variable_map.count("create_owner");
		std::cout << std::flush;
		state = new shared_state(&server, state_config, create_owner_account);
		// state = new shared_state(server.db, server.document_root, server.media_location, state_config, std::move(busted_target_to_target), std::move(files_generated_from_templates));
		state->start();
	}
	catch (const std::exception& exception) {
		std::cerr << "[shared_state] " << exception.what() << std::endl;
		return 1;
	}
	server.run(state);


	return EXIT_SUCCESS;
}
