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
	("favicon_url", std::string("https://fuze.page/favicon.ico"))
	("site_name", &state_config->server_name, {.default_value=std::string("FuzeHttp Example")})
	("test_program_constant", 73, {.is_option = false})
	;
}
} // export namespace FuzeHttp::Example
