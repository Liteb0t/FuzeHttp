// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <boost/container/container_fwd.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <sodium.h>
#include <boost/hash2/md5.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include <boost/smart_ptr.hpp>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
export module FuzeHttp.Utils;

export namespace FuzeHttp{

inline bool fileNameEndsWith(const std::string& file_name, const std::string& delimiter) {
	int file_extension_index;
	if ((file_extension_index = file_name.rfind(".")) == -1) {
		file_extension_index = file_name.size();
	}
	return file_name.substr(0, file_extension_index).ends_with(delimiter);
}

inline std::string insertExtensionToFileName(const std::string& file_name, const std::string& extension) {
	int file_extension_index;
	if ((file_extension_index = file_name.rfind(".")) == -1) {
		file_extension_index = file_name.size();
	}
	std::string new_file_name = file_name.substr(0, file_extension_index) + extension + file_name.substr(file_extension_index);
	return new_file_name;
}

inline bool isValidURLParameter(const std::string& parameter) {
	if (parameter.length() == 0) return false;
	// https://stackoverflow.com/a/2926983/18658154
	return find_if(parameter.begin(), parameter.end(),
		[](char c) { return !(isalnum(c) || (c == '_') || (c == '-')); }) == parameter.end();
}

template<typename BoostHashType, typename StringType>
requires (requires(BoostHashType hasher, const StringType& str){hasher.update(str.c_str(), str.length());})
std::string getHash(const StringType& source_data) {
	BoostHashType hash;
	hash.update(source_data.c_str(), source_data.length());
	char hash_base64[sodium_base64_ENCODED_LEN(128 / 8, sodium_base64_VARIANT_URLSAFE_NO_PADDING)];
	sodium_bin2base64(
		hash_base64, sizeof hash_base64,
		hash.result().data(), hash.result().size(),
		// (unsigned char*)key_bytes, 20,
		sodium_base64_VARIANT_URLSAFE_NO_PADDING
	);
	return hash_base64;
}

inline std::string getEtagFromFile(const std::filesystem::path& file) {
	std::string file_last_modified = std::to_string(std::filesystem::last_write_time(file).time_since_epoch().count());
	return getHash<boost::hash2::md5_128>(file_last_modified);
}

std::string writeManifestJson(const std::filesystem::path& manifest_file, const std::unordered_map<std::string /*target*/, std::string /*etag*/>& manifest_frontend_etags, const std::string& combined_hash) {
	boost::json::object manifest_obj;
	boost::json::object manifest_frontend_obj;
	for (const auto& target : manifest_frontend_etags)
		manifest_frontend_obj.emplace(target.first, target.second);
	manifest_obj.emplace("frontend", manifest_frontend_obj);
	manifest_obj.emplace("combined_hash", combined_hash);
	std::ofstream manifest_json_out(manifest_file);
	std::string json_as_str = boost::json::serialize(manifest_obj);
	manifest_json_out.write(json_as_str.c_str(), json_as_str.length());
	return json_as_str;
}
}; // namespace FuzeHttp
