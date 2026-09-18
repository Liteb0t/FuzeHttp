// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <string>
export module FuzeHttp.Example.Config;
import FuzeHttp.Example.State;
import FuzeHttp.PermissionObject;
import FuzeHttp.ProgramOptions;

export namespace FuzeHttp::Example {
void addProgramOptions(FuzeHttp::ProgramOptions* options, StateConfig* state_config) {
	options->addOptions()
	(new ProgramOptionPtr("site_name", &state_config->server_name, {.default_value=std::string("FuzeHttp Example")}))
	(new ProgramOption<std::string>("favicon_url", "https://fuze.page/favicon.ico"))
	(new ProgramConstant("group_max_name", static_cast<int>(Group::MAX_NAME)))
	(new ProgramConstant("account_max_username", static_cast<int>(Account::MAX_USERNAME)))
	;
}
} // export namespace FuzeHttp::Example
