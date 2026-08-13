module;
// #include "FuzeMigrationHelper.hpp"
#include <iostream>
#include <filesystem>
#include <functional>
#include <list>
#include <string>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <print>
export module FuzeHttp.Migrations;
import FuzeDBI;

export namespace FuzeHttp {
namespace Migrations {
class Migration {
public:
	Migration(const std::string version_string) : version_string(version_string) {}
	virtual void makeMigration(FuzeDBI::Connection* fuze_dbi) { std::println("TEST migrating to {}", version_string); };
	const std::string version_string;
};
class SQLOnlyMigration : public Migration {
public:
	SQLOnlyMigration(const std::string version_string, const std::string sql_statement) : Migration(version_string), sql_statement(sql_statement) {}
	void makeMigration(FuzeDBI::Connection* fuze_dbi) override {
		std::println("[Migrations] SQLOnlyMigration sql_statement: {}", sql_statement);
		fuze_dbi->query<void>(sql_statement);
	}
	const std::string sql_statement;
};
template<class StateType>
class SmartMigration : public Migration {
public:
	// SmartMigration(const std::string version_string, const void(*function)(int blaaaarg), int blaaaarg) : Migration(version_string), blaaaarg(blaaaarg), function(function) {}
	template<typename Callback>
	SmartMigration(const std::string version_string, StateType* state, Callback&& func) : Migration(version_string), blaaaarg(state), func(std::forward<Callback>(func)) {}
	void makeMigration(FuzeDBI::Connection* fuze_dbi) override {
		std::println("[Migrations] SmartMigration");
		this->func(fuze_dbi, blaaaarg);
	}
	// const void(*function)(int blaaaarg);
	std::function<void(FuzeDBI::Connection*, StateType*)> func;
	StateType* blaaaarg;
};
// std::list<std::unique_ptr<Migration>> migrations;
void makeNewMigrations(FuzeDBI::Connection* fuze_dbi, const std::string& database_version_string, const std::string& server_version, const std::list<std::unique_ptr<Migration>>&& migrations) {
	int migrations_needed = 0; // 0=no, 1=yes (prompt), 2=yes (user accepted prompt)
	for (const std::unique_ptr<Migration>& migration : migrations) {
		if (migration->version_string > server_version) {
			throw std::runtime_error("There is an error in configuration for migrations; migration has newer version than server_version");
		}
		if (database_version_string < migration->version_string) {
			if (migrations_needed != 2) {
				migrations_needed = 1;
				std::println("Database migrations need to be made. It is recommended to backup the database before proceeding.");
				std::print("Proceed? (Y/n): ");
				std::string response;
				std::getline(std::cin, response);
				if (response.empty() || response[0] == 'Y' || response[0] == 'y')
					migrations_needed = 2;
				else {
					std::println("Database migration cancelled by user");
					break;
				}
			}
			migration->makeMigration(fuze_dbi);
		}
	}

	fuze_dbi->query<void>("UPDATE _info SET version = $1", server_version);
}
// Populates database with entries in database_template.sql, and sets the version
void firstTimeSetup(FuzeDBI::Connection* fuze_dbi, const std::filesystem::path& template_path, const std::filesystem::path& absolute_sqlite_path, const std::string& current_version) {
	int ec; char* error_message;
	std::println("Doing first-time setup");
	std::println("Opening database template at {}", template_path.string());
	if (!std::filesystem::exists(template_path))
		throw std::runtime_error("Error: database template not found");
#ifdef FUZEDBI_POSTGRES
	std::println("It appears you are setting up the PostgreSQL database for the first time. Please ensure that the database is empty and the user has full read/write permissions.");
	std::print("Proceed? (Y/n): ");
	std::string response;
	std::getline(std::cin, response);
	if (!(response.empty() || response[0] == 'Y' || response[0] == 'y'))
		throw std::runtime_error("Database setup cancelled by user");
#endif
	std::ifstream sqlite_template_file(template_path.string());
	std::string line;
	try {
		while (std::getline(sqlite_template_file, line)) {
			std::cout << "[Migrations] " << line << std::endl;
			if (!line.empty()) {
				fuze_dbi->query<void>(line);
			}
		}
		fuze_dbi->query<void>("CREATE TABLE IF NOT EXISTS _info(version TEXT NOT NULL)");
		fuze_dbi->query<void>("INSERT INTO _info(version) VALUES ($1)", current_version);
	}
	catch (std::exception& exception) {
		std::cout << "[Migrations] Exception in DB init: " << exception.what() << std::endl;
#ifdef FUZEDBI_SQLITE
		std::cout << "Remove SQLite database file? (Y/n) ";
		std::string do_remove;
		std::cin >> do_remove;
		if (do_remove.empty() || do_remove[0] == 'Y' || do_remove[0] == 'y')
			std::remove(absolute_sqlite_path.string().c_str());
#endif
		throw std::runtime_error("An error occured during database template import.");
	}
}
} // Migrations
} // FuzeHttp
