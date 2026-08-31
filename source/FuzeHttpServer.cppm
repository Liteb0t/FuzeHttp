module;
// #include "Listener.hpp"
#include <boost/asio.hpp>
#define BOOST_DLL_USE_STD_FS
#include <boost/algorithm/string/replace.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/hash2/md5.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include <boost/smart_ptr.hpp>
#include <iostream>
#include <list>
#include <print>
#include <sodium.h>
#include <unordered_map>
#include <unordered_set>
export module FuzeHttp.Server;
// #include "FuzeHttpUtils.hpp"
import FuzeDBI;
import FuzeHttp.Listener;
import FuzeHttp.Migrations;
import FuzeHttp.PermissionObject;
import FuzeHttp.State;
import FuzeHttp.Utils;

namespace FuzeHttp {
	struct ProgramDirectories {
		std::filesystem::path data;
		std::filesystem::path media;
		std::filesystem::path sqlite_file;
	};

	std::filesystem::path getConfigDirectory(std::filesystem::path program_location, std::optional<std::string> config_file, std::optional<std::string> data_directory_config, const std::string& data_folder_name) {
	std::filesystem::path config_path;
	if (config_file) { // Line set in cmdline options
		config_path = config_file.value();
	}
	// XDG_DATA_HOME directories are only used in the AppImage distribution. Maybe change in the future.
	else if (std::getenv("APPDIR")) {
		if (data_directory_config)
			config_path = std::filesystem::path(data_directory_config.value()) / "config.ini";
		else if (const char* xdg_data_home = std::getenv("XDG_DATA_HOME"))
			config_path = std::filesystem::path(xdg_data_home) / data_folder_name / "config.ini";
		else if (const char* unix_home = std::getenv("HOME"))
			config_path = std::filesystem::path(unix_home) / ".local" / "share" / data_folder_name / "config.ini";

		if (!std::filesystem::exists(config_path)) {
			std::filesystem::path data_directory = std::filesystem::absolute(program_location / ".." / "share" / data_folder_name); // To match Unix

			std::println("Copying config.ini from AppImage to {}", config_path.string());
			if (!std::filesystem::exists(data_directory / "config.ini"))
				throw std::runtime_error("config.ini not found in AppImage data directory.");
			else {
				std::filesystem::create_directories(config_path.parent_path());
				std::filesystem::copy(data_directory / "config.ini", config_path);
			}
		}
	}
	else if (data_directory_config) {
		config_path = std::filesystem::path(data_directory_config.value()) / "config.ini";
	}
	else {
		config_path = std::filesystem::absolute(program_location / ".." / "share" / data_folder_name / "config.ini");
	}
	return config_path;
}

std::optional<ProgramDirectories> getProgramDirectories(std::filesystem::path program_location, std::optional<std::string> data_directory_config, std::optional<std::string> media_directory_config, std::optional<std::string> sqlite_database_file_config, const std::string& data_folder_name) {
	std::filesystem::path data_directory; // Typically in ~/.local/share/FuzeMediaboard, except for AppImage
	std::filesystem::path writeable_directory; // Different from data_directory in AppImage
	// std::filesystem::path config_file; // Different from data_directory in AppImage
	// Get the path to this program, so files can be read/written relative to the executable
	if (data_directory_config)
		writeable_directory = data_directory_config.value();
	else if (std::getenv("APPDIR")) {
		if (const char* xdg_data_home = std::getenv("XDG_DATA_HOME"))
			writeable_directory = std::filesystem::path(xdg_data_home) / data_folder_name;
		else if (const char* unix_home = std::getenv("HOME"))
			writeable_directory = std::filesystem::path(unix_home) / ".local" / "share" / data_folder_name;
		else
			throw std::runtime_error("Running from AppImage requires XDG_DATA_HOME or HOME environment variables.");
	}
	else
		writeable_directory = std::filesystem::absolute(program_location / ".." / "share" / data_folder_name); // For development


	std::filesystem::create_directories(writeable_directory);
	// std::filesystem::create_directories(config_file.parent_path());

	if (std::getenv("APPDIR")) {
		data_directory = std::filesystem::absolute(program_location / ".." / "share" / data_folder_name); // To match Unix
		// if (!std::filesystem::exists(config_file)) {
		// 	std::println("Copying config.ini from AppImage");
		// 	std::filesystem::copy(data_directory / "config.ini", config_file);
		// }
	}
	else {
		data_directory = writeable_directory;
	}
	std::filesystem::path sqlite_file;
	if (sqlite_database_file_config) {
		sqlite_file = sqlite_database_file_config.value();
		if (std::filesystem::is_directory(sqlite_file))
			sqlite_file += "mediaboard_sqlite_data.db";
	}
	else
		sqlite_file = writeable_directory / "sqlite_data.db";
	std::filesystem::path media_directory;
	if (media_directory_config)
		media_directory = media_directory_config.value();
	else
		media_directory = writeable_directory / "media";
	std::filesystem::create_directories(media_directory / "thumbnails");
	return ProgramDirectories{
		.data = data_directory,
		.media = media_directory,
		.sqlite_file = sqlite_file
	};
}

// Mysteriously doesnt link when placed in cpp file
inline void applyOptionsToTemplates(const std::vector<TemplateMacro*>& options, const std::filesystem::path& document_root, const std::unordered_map<std::string /*target*/, std::string /*etag*/> manifest_frontend_etags){
	// std::println("Adding options to templates...");
	for (auto option : options)
		std::println("{} :: {}", option->token, option->string());
	for (const std::filesystem::directory_entry& dir_entry : std::filesystem::recursive_directory_iterator(document_root)) {
		if (!std::filesystem::is_regular_file(dir_entry))
			continue;
		int file_extension_index;
		if ((file_extension_index = dir_entry.path().filename().string().rfind(".")) == -1) {
			file_extension_index = dir_entry.path().filename().string().size();
		}
		if (!dir_entry.path().filename().string().substr(0, file_extension_index).ends_with(".template"))
			continue;
		// std::println("[applyOptionsToTemplates] path: {}", dir_entry.path().string());
		std::ifstream file_template_stream(dir_entry.path());
		std::string out_filename = dir_entry.path().filename().string().substr(0, file_extension_index - sizeof(".template")+1) + ".GENERATED" + dir_entry.path().filename().string().substr(file_extension_index);
		// if (out_filename.starts_with('_'))
		// 	out_filename = out_filename.substr(1);
		// std::println(" ->{} ", out_filename);
		std::ofstream file_output_stream(dir_entry.path().parent_path() / out_filename);
		std::string file_line;
		while (std::getline(file_template_stream, file_line)) {
			for (auto option : options) {
				if (option->includeInFrontend())
					boost::replace_all(file_line, std::format("CONFIG_{}", option->token), option->string());
			}
			if (size_t file_token_i; (file_token_i = file_line.find("FILE_")) != std::string::npos) {
				size_t file_token_value_i = file_token_i + sizeof "FILE_";
				// std::println("{}", file_line[file_token_value_i]);
				if (file_line[file_token_value_i - 1] != '"')
					throw std::runtime_error("FILE_ macro requires \" characters around path");
				size_t closing_index = file_line.find('"', file_token_value_i);
				// std::println("{}", file_line[closing_index]);
				if (closing_index == std::string::npos)
					throw std::runtime_error("FILE_ macro missing closing \" character");
				std::string file_token_value = file_line.substr(file_token_value_i, closing_index - file_token_value_i);
				// std::println("file_token_value: {}", file_token_value);
				std::filesystem::path file_token_path = file_token_value;
				// std::println("file_token_path: {}", file_token_value);
				std::filesystem::path resolved_file_token_path = dir_entry.path().parent_path() / file_token_path;
				// std::println("resolved_file_token_path: {}", resolved_file_token_path.string());
				std::filesystem::path proximate_file_token_path = std::filesystem::proximate(resolved_file_token_path, document_root);
				if (auto it = manifest_frontend_etags.find(proximate_file_token_path.string()); it != manifest_frontend_etags.end()) {
					// std::println("Found manifest etag! {}", it->second);
					file_line.erase(file_token_i, closing_index+1 - file_token_i);
					file_line.insert(file_token_i, insertExtensionToFileName(file_token_value, it->second));
				}
				else
					throw std::runtime_error(std::format("Etag not found\nPath: {}\n Line: {}", proximate_file_token_path.string(), file_line));


				// std::filesystem::path proximate_file_token_path = std::filesystem::proximate(file_token_path);
				// std::println("resolved_file_token_path: {}", resolved_file_token_path.string());
				// TODO replace file_token_value with cache-busted version by finding path from map
				// std::filesystem::path dependency_path = file_token_value;
			}
			file_output_stream << file_line << std::endl;
		}
		file_output_stream.close();
		file_template_stream.close();
	}
	std::println("Done.");
}
} // namespace FuzeHttp

export namespace FuzeHttp {

template<class StateType, class WebsocketSessionType = WebsocketSession>
class Server {
public:
	Server(const std::string current_version) : current_version(current_version) {
		if (sodium_init() < 0) {
			std::println(std::cerr, "libsodium couldn't be initialised");
			return;
		}
	}
	int processOptions(int argc, char* argv[], std::vector<FuzeHttp::TemplateMacro*> additional_options, const std::string& data_folder_name) {
		std::error_code ec;
		std::filesystem::path program_location = boost::dll::program_location().parent_path();
		if (ec)
			throw std::runtime_error("An error occured when attempting to get the current program's location.");
		else
			std::println("Server is located at {}", program_location.string());

		std::optional<std::string> config_file, data_directory_config, media_directory_config, sqlite_database_file_config;
		// Check command line arguments.
		unsigned short server_port, postgresql_port;
		std::string config_file_str, data_directory_str, media_directory_str, database_engine, environment_variable_for_secret, sqlite_database_file_str, postgresql_uri, postgresql_user, postgresql_host, thumbnail_file_format, postgresql_database_name;
		unsigned int threads, thumbnail_size, parser_body_size_limit_mb;
		bool postgresql_use_uri, secret_required;
		boost::program_options::options_description command_line_specific_options("Command-line-specific options");
		command_line_specific_options.add_options()
			("create_owner,o", "Generates a link to create the server owner's account.")
			("config,c", boost::program_options::value<std::string>(&config_file_str), "location of configuration file.")
			("version,v", "Show version string.")
			("help,h", "Show list of options.");

		// std::string site_name, favicon_url;
		// boost::shared_ptr<boost::program_options::option_description> desc( new boost::program_options::option_description("site_name", boost::program_options::value<std::string>(&site_name)));

		// TemplateOption favicon_url_opt("favicon_url", &favicon_url);

		// These options can be specified in config.ini
		boost::program_options::options_description universal_options("Universal options");
		universal_options.add_options()
			("data_directory", boost::program_options::value<std::string>(&data_directory_str))
			// ("file_size_limit_mb", boost::program_options::value<unsigned int>(&state_config.file_size_limit_mb)->default_value(25), "In MB")
			("environment_variable_for_secret", boost::program_options::value<std::string>(&environment_variable_for_secret)->default_value("FUZEHTTP_SECRET"))
			("secret_required", boost::program_options::value<bool>(&secret_required)->default_value(true))
			("media_directory,m", boost::program_options::value<std::string>(&media_directory_str),  "File path where user-submitted media is stored. data_directory is used if none is specified.")
			("sqlite_database_file,s", boost::program_options::value<std::string>(&sqlite_database_file_str),  "File where SQLite data is stored. data_directory is used if none is specified.")
			("server_port,p", boost::program_options::value<unsigned short>(&server_port)->default_value(8300), "The port which the server will serve. Make sure it isn't already in use by another service.")
			("parser_body_size_limit_mb", boost::program_options::value<unsigned int>(&parser_body_size_limit_mb)->default_value(100), "Maximum HTTP body in megabytes")
			("postgresql_use_uri", boost::program_options::value<bool>(&postgresql_use_uri)->default_value(false), "If true, use postgresql_uri to connect.")
			("postgresql_uri,u", boost::program_options::value<std::string>(&postgresql_uri)->default_value("fuze_mediaboard@localhost:5432"),  "Connection string for the PostgreSQL database.")
			("postgresql_user,U", boost::program_options::value<std::string>(&postgresql_user)->default_value("mediaboard_server"),  "User which will access the PostgreSQL database.")
			("postgresql_host,h", boost::program_options::value<std::string>(&postgresql_host)->default_value("localhost"),  "Host for the PostgreSQL database.")
			("postgresql_port,p", boost::program_options::value<unsigned short>(&postgresql_port)->default_value(5432), "The port at which the database is available.")
			("postgresql_database_name,n", boost::program_options::value<std::string>(&postgresql_database_name)->default_value("fuze_mediaboard"), "Name of the PostgreSQL database.")
			("threads,t", boost::program_options::value<unsigned int>(&threads)->default_value(1), "Number of async threads. For now, only use 1 in production.");
			// ("thumbnail_file_extension", boost::program_options::value<std::string>(&state_config.thumbnail_file_extension)->default_value("jpg"), "File format in which ImageMagick will create thumbnails.");
			// ("thumbnail_size", boost::program_options::value<unsigned int>(&state_config.thumbnail_size)->default_value(150), "Maximum width and height of image thumbnails, in pixels.");

		for (auto option : additional_options) {
			option->addOptionToListIfOptional(universal_options);
		}

		boost::program_options::options_description command_line_options;
		command_line_options.add(command_line_specific_options).add(universal_options);
		std::filesystem::path config_file_path;

		// boost::program_options::variables_map this->variable_map;
		try {
			store(boost::program_options::parse_command_line(argc, argv, command_line_options), this->variable_map);
			boost::program_options::notify(this->variable_map);

			if (this->variable_map.count("config"))
				config_file = config_file_str;
			config_file_path = getConfigDirectory(program_location, config_file, data_directory_config, data_folder_name);
			// Load config.ini
			std::ifstream config_file_ifstream(config_file_path.string());
			if (config_file_ifstream) {
				std::println("Loaded config file {}", config_file_path.string());
				store(parse_config_file(config_file_ifstream, universal_options), this->variable_map);
				boost::program_options::notify(this->variable_map);
			}
			else {
				std::println("Could not find config.ini file. Default options will be used.");
			}
		}
		catch (const std::exception& exception) {
			std::println("{}", exception.what());
			return 1;
		}

		if (this->variable_map.count("help")) {
			std::cout << command_line_options << std::endl;
			return 2;
		}
		if (this->variable_map.count("version")) {
			std::println("{}", current_version);
			return 2;
		}
		if (this->variable_map.count("data_directory"))
			data_directory_config = data_directory_str;
		if (this->variable_map.count("media_directory")) {
			std::println("media_directory config option found");
			media_directory_config = media_directory_str;
		}
		else
			std::println("media_directory config option not found");
		if (this->variable_map.count("sqlite_database_file"))
			sqlite_database_file_config = sqlite_database_file_str;
		std::optional<ProgramDirectories> program_directories_opt = getProgramDirectories(program_location, data_directory_config, media_directory_config, sqlite_database_file_config, data_folder_name);
		if (!program_directories_opt) {
			std::println(std::cerr, "Mediaboard setup was cancelled by the user.");
			return 3;
		}
		else
			program_directories = program_directories_opt.value();
		std::println("Data:\t {}", program_directories.data.string());
#ifdef FUZEDBI_SQLITE
		std::println("SQLite:\t {}", program_directories.sqlite_file.string());
#endif
		std::println("Media:\t {}", program_directories.media.string());

		std::println("FuzeDBI interface: {}", FUZEDBI_DB);
		// FuzeDBI::Connection* fuze_database_interface;
		try {
#ifdef FUZEDBI_POSTGRES
			this->db = new FuzeDBI::Connection(postgresql_user, postgresql_host, postgresql_port, postgresql_database_name);
#elifdef FUZEDBI_SQLITE
			std::print("sqlite_database_file: {}", program_directories.sqlite_file.string());

			this->db = new FuzeDBI::Connection(program_directories.sqlite_file.string());
#endif
			try {
				database_version = this->db->query<std::optional<std::string>>("SELECT version FROM _info");
			}
			catch (const std::exception e) {
				std::println("Version string not found");
			}
			if (!database_version) {
				// std::filesystem::path template_path = std::filesystem::absolute("database_template.sql", database_location);
				// if (!std::filesystem::exists(template_path))
				// 	throw std::runtime_error(std::format("Database template file {} not found.", template_path.string()));
				Migrations::firstTimeSetup(this->db, program_directories.data / "database_template.sql", program_directories.sqlite_file.string(), current_version);
			}
		}
		catch (const std::exception& exception) {
			std::println(std::cerr, "{}", exception.what());
			return 1;
		}
		std::println("Set port: {}", server_port);
		std::println("Set threads: {}", threads);
		if (threads > 1)
			std::println("Warning: issues may arise from multi-threading");

		// this->media_location = program_directories.media;
		this->document_root = program_directories.data / "frontend";
		std::filesystem::path manifest_file = program_directories.data / "manifest.json";
		// std::filesystem::path template_root = program_directories.data / "frontend" / "templates";
		// boost::bimap<std::string, std::string> path_to_busted_path;
		// std::unordered_map<std::string, std::filesystem::path> busted_target_to_path;

		// std::unordered_map<std::string , std::string > manifest_frontend_etags;
		// std::unordered_set<std::string> files_generated_from_templates;

		// cache controle to major steve
		std::unordered_map<std::string /*target*/, std::string /*etag*/> manifest_frontend_etags;
		std::unordered_map<std::string, std::string> busted_target_to_target;
		std::unordered_set<std::string> files_generated_from_templates;
		std::string frontend_etag; // Changes when any frontend file changes, ensuring client refreshes cache.
		try {
			std::optional<std::string> old_combined_hash;
			bool manifest_file_existed;
			if (std::filesystem::exists(manifest_file)) {
				manifest_file_existed = true;
				std::ifstream manifest_json_in(manifest_file);
				std::string file_line, json_as_str;
				while (std::getline(manifest_json_in, file_line))
					json_as_str += file_line;
				boost::json::object manifest_obj = boost::json::parse(json_as_str).as_object();
				for (const auto& frontend_json_entry : manifest_obj.at("frontend").as_object()) {
					std::filesystem::path frontend_file_path = std::string(frontend_json_entry.key());
					if (!std::filesystem::is_regular_file(frontend_file_path))
						continue;
					if (fileNameEndsWith(frontend_file_path.filename(), ".template"))
						continue;
					if (fileNameEndsWith(frontend_file_path.filename(), ".GENERATED"))
						continue;
					// std::filesystem::path frontend_file_path = std::filesystem::proximate(frontend_file.path(), document_root);
					if (!manifest_frontend_etags.contains(frontend_file_path.string()))
						manifest_frontend_etags.emplace(std::string(frontend_json_entry.key()), frontend_json_entry.value().as_string());
				}
				old_combined_hash = manifest_obj.at("combined_hash").as_string();
			}
			else
				manifest_file_existed = false;
			// create manifest JSON OBJECT
			// All frontend files except GENERATED are added to manifest. To detect changes the manifest JSON in memory and the previously used one in the filesystem are hashed; if the hashes are not equal, we know there was a change.
			boost::json::object manifest_obj, manifest_frontend_json_obj, manifest_options_json_obj;
			for (const auto& frontend_file : std::filesystem::recursive_directory_iterator(document_root)) {
				if (!std::filesystem::is_regular_file(frontend_file))
					continue;
				std::filesystem::path frontend_file_path = std::filesystem::proximate(frontend_file.path(), document_root);
				if (!manifest_frontend_etags.contains(frontend_file_path.string())) {
					if (!fileNameEndsWith(frontend_file.path().filename(), ".GENERATED")) {
						std::string new_etag = getEtagFromFile(frontend_file);
						if (!fileNameEndsWith(frontend_file.path().filename(), ".template"))
							manifest_frontend_etags.emplace(frontend_file_path.string(), new_etag);
						manifest_frontend_json_obj.emplace(frontend_file_path.string(), new_etag);
					}
				}
			}
			for (auto option : additional_options) {
				if (option->includeInFrontend())
					manifest_options_json_obj.emplace(option->token, option->string());
			}
			// std::string config_hash = getHash<boost::hash2::md5_128>(std::to_string(std::filesystem::last_write_time(config_file_path).time_since_epoch().count()));
			std::string options_hash = getHash<boost::hash2::md5_128>(boost::json::serialize(manifest_options_json_obj));
			std::string frontend_hash = getHash<boost::hash2::md5_128>(boost::json::serialize(manifest_frontend_json_obj));
			frontend_etag = frontend_hash;
			std::string new_combined_hash = getHash<boost::hash2::md5_128>(options_hash + frontend_hash);
			manifest_obj.emplace("frontend", manifest_frontend_json_obj);
			manifest_obj.emplace("combined_hash", new_combined_hash);
			std::string new_json_as_str = boost::json::serialize(manifest_obj);
			std::ofstream manifest_json_out(manifest_file);
			manifest_json_out.write(new_json_as_str.c_str(), new_json_as_str.length());
			// if combined_hash is different, write to file and process templates

			// writeManifestJson(manifest_file, manifest_frontend_etags, config_file_path);

			// json_as_str = writeManifestJson(manifest_file, manifest_frontend_etags);


			if (!old_combined_hash || (old_combined_hash.value() != new_combined_hash))
				FuzeHttp::applyOptionsToTemplates(additional_options, document_root, manifest_frontend_etags);
			else
				std::println("No changes to frontend detected.");

			for (const auto& frontend_file : std::filesystem::recursive_directory_iterator(document_root)) {
				if (!std::filesystem::is_regular_file(frontend_file))
					continue;
				std::filesystem::path frontend_file_path = std::filesystem::proximate(frontend_file.path(), document_root);
				// TODO fix bug where two starts are required to add to files_generated_from_templates
				if (fileNameEndsWith(frontend_file_path.filename(), ".GENERATED")) {
					files_generated_from_templates.emplace(
						frontend_file_path.string().substr(0, frontend_file_path.string().rfind(".GENERATED")) +
						frontend_file_path.string().substr(frontend_file_path.string().rfind('.')));
				}
			}
		}
		catch (const std::exception& exception) {
			std::println(std::cerr, "An error occured when generating frontend files: {}", exception.what());
			return 1;
		}
		for (const auto& target : manifest_frontend_etags)
			busted_target_to_target.emplace(FuzeHttp::insertExtensionToFileName(target.first, target.second), target.first);
		for (const std::string& target : files_generated_from_templates)
			std::println("Target to file generated from template: {}", target);

		// state
		this->state = std::make_unique<StateType>(db);
		if (this->variable_map.count("create_owner")) {
			std::string invite_key = state->createInvite(static_cast<int>(BUILTIN_GROUPS::OWNER));
			std::println("\nUse this link to register the owner account: http://localhost:{}/invite/{}", this->server_port, invite_key);
		}
		else if (!state->ownerExists())
			std::println("\nERROR: No owner found. Restart the application with --create_owner");
		state->document_root = document_root;
		state->setSecretFromEnvironmentVariable(environment_variable_for_secret, secret_required);
		state->media_location = program_directories.media;
		state->busted_target_to_target = std::move(busted_target_to_target);
		// std::println("Busted target to target:");
		// for (const auto& target :state-> busted_target_to_target)
		// 	std::println("{} :: {}", target.first, target.second);
		state->manifest_frontend_etags = std::move(manifest_frontend_etags);
		// std::print("manifest_frontend_etags: ");
		// for (const auto& target : state->manifest_frontend_etags)
		// 	std::println("{} :: {}", target.first, target.second);
		state->files_generated_from_templates = std::move(files_generated_from_templates);
		state->frontend_etag = frontend_etag; // Changes when any frontend file changes, ensuring client refreshes cache.
		// this->state = std::move(state);
		// std::println("frondend_etag: {}", state->frontend_etag);
		state->parser_body_size_limit_mb = parser_body_size_limit_mb;
		return -1;
	}

	void run() {
		auto migrations = state->addMigrations();
		std::println("Server version:   \t{}", this->current_version);
		if (database_version) {
			std::println("Database version: \t{}", database_version.value());
			// std::println("Database version: {}", database_version.value());
			// Migrations::makeMigrations(this->db, database_version.value(), current_version);
			Migrations::makeNewMigrations(this->db, database_version.value(), current_version, std::move(migrations));
		}
		else
			std::println("Database version: \tNot applicable (database newly created)");
		state->start();

		auto address = boost::asio::ip::make_address("127.0.0.1");
		// The io_context is required for all I/O - see https://www.boost.org/doc/libs/latest/doc/html/boost_asio/overview/basics.html
		boost::asio::io_context io_context;
			// Create and launch a listening port
		std::println("Creating a listening port...");
		std::make_shared<Listener<StateType, WebsocketSessionType>>(
			io_context,
			boost::asio::ip::tcp::endpoint{address, server_port},
			state.get()
		)->run();

		// Capture SIGINT and SIGTERM to perform a clean shutdown
		std::println("Setting signals...");
		boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
		signals.async_wait(
			[&io_context](boost::system::error_code const&, int) {
				// Stop the io_context. This will cause run()
				// to return immediately, eventually destroying the
				// io_context and any remaining handlers in it.
				io_context.stop();
			}
		);


		// Run the I/O service on the requested number of threads
		std::println("Running the I/O service...");
		std::println("The server can now be accessed from http://localhost:{}", server_port);
		std::vector<std::thread> v;
		v.reserve(threads - 1);
		for(auto i = threads - 1; i > 0; --i) {
			v.emplace_back(
				[&io_context] {
					io_context.run();
				}
			);
		}
		io_context.run();

		// (If we get here, it means we got a SIGINT or SIGTERM)

		// Block until all the threads exit
		for(auto& t : v)
			t.join();
		if (threads == 1)
			std::println("Thread closed.");
		else
			std::println("All {} threads closed.", threads);
		state->clearExpiredSessions();
		// delete state;
		// delete database_connection;
	}
	FuzeDBI::Connection* db;
	std::unique_ptr<StateType> state;
	// StateType state = nullptr;
	ProgramDirectories program_directories;
	std::filesystem::path document_root;
	// std::filesystem::path media_location;
	boost::program_options::variables_map variable_map;
	const unsigned short server_port = 8300;
	std::optional<std::string> database_version;
	const std::string current_version;
private:
	// FuzeHttp::StateBase* state;
	const unsigned short threads = 1;
};
} // namespace FuzeHttp
