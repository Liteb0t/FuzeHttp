module;
#include "shared_state.hpp"
#include <list>
#include <memory>
#include <print>
export module FuzeHttp.Example.Migrations;
import FuzeHttp.Migrations;
using namespace FuzeHttp::Migrations;

export std::list<std::unique_ptr<Migration>> addMigrations(shared_state* state) {
	std::list<std::unique_ptr<Migration>> migrations;
	/* Example migrations:
	migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.1.1",
		"ALTER TABLE message_file ADD COLUMN width INTEGER;"
		"ALTER TABLE message_file ADD COLUMN height INTEGER;")));
	migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.1.2",
		"ALTER TABLE message_file ADD COLUMN thumbnail_file_extension TEXT;"
		"ALTER TABLE thread ADD COLUMN message_id_seq INTEGER DEFAULT 0;"
		"UPDATE thread SET message_id_seq = 1000")));
	migrations.push_back(std::unique_ptr<Migration>(new SmartMigration("0.2.2", state, [](FuzeDBI::Connection* db, shared_state* state){
		const std::string version_string = db->query<std::string>("SELECT version FROM _info");
		std::println("This is the lambda and document_root is {} and version string is {}", state->getDocumentRoot().string(), version_string);
	})));
	*/
	return migrations;
}
