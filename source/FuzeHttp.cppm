// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
module;
#include <boost/utility/string_view.hpp>
#include <boost/beast/core/string_type.hpp>
// #include "PermissionObject.hpp"
#include <boost/beast/http/status.hpp>
#include <beast.hpp>
#include <sodium.h>
#include <charconv>
#include <filesystem>
#include <print>
#include <string>
#include <string_view>
#include <sys/un.h>
#include <unordered_map>
#include <unordered_set>
#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <variant>
#include <tuple>
#include <vector>
#include <type_traits> // For std::conditional_t
export module FuzeHttp.Core;
export import FuzeHttp.PermissionObject;

namespace beast = boost::beast;
namespace http = beast::http;                   // from <boost/beast/http.hpp>
export namespace FuzeHttp {
// URL decoding in C http://www.geekhideout.com/urlcode.shtml
char fromHex(char ch) {
	return std::isdigit(ch) ? ch - '0' : std::tolower(ch) - 'a' + 10;
}

std::string getDecodedURL(boost::string_view raw_URL) {
	// URL decoding in C http://www.geekhideout.com/urlcode.shtml
	std::string decoded_url;
	decoded_url.reserve(raw_URL.length()+1);
	for (boost::string_view::const_iterator i = raw_URL.begin(), n = raw_URL.end(); i != n; i++) {
		std::string::value_type c = (*i);
		if (c == '%') {
			if (i+1 != n && i+2 != n) {
				decoded_url += fromHex(*(i+1)) << 4 | fromHex(*(i+2));
				i += 2;
			}
		}
		else if (c == '+')
			decoded_url += ' ';
		else
			decoded_url +=  c;
	}
	return decoded_url;
}
std::string_view getPathName(const std::string& source_URL) {
	// path_name excludes URL parameters (stuff after '?')
	// removes trailing / but leaves first /
	std::string_view path_name = source_URL;
	int decoded_url_questionmark_index = source_URL.rfind('?');
	if (decoded_url_questionmark_index != std::string::npos)
		path_name = path_name.substr(0, decoded_url_questionmark_index);
	return path_name;
}

const std::string forbidden_file_name_chars = "#?+/&";
void sanitiseFileName(std::string& file_name) {
	if (file_name[0] == ' ')
		file_name[0] = '_';
	for (int i = 0; i < file_name.length(); i++) {
		if (forbidden_file_name_chars.find(file_name[i]) != -1) {
			file_name[i] = '_';
		}
	}
}

inline std::string formatCookie(const std::string& session_id, int max_age) {
	return std::format("Session={}; Path=/; HttpOnly; Max-Age={}", session_id, max_age);
}

// Cookie without Max-Age expires on session end. See: https://developer.mozilla.org/en-US/docs/Web/HTTP/Guides/Cookies#removal_defining_the_lifetime_of_a_cookie
inline std::string formatCookie(const std::string& session_id) {
	return std::format("Session={}; Path=/; HttpOnly", session_id);
}

struct Session {
	const int id;
	const int client_id;
	// const std::string key;
	const std::chrono::time_point<std::chrono::system_clock> created_at;
};

struct Invite {
	const int granted_group_id;
	const std::chrono::time_point<std::chrono::system_clock> created_at;
};
/*
template<typename... Option>
struct Requires {};*/

struct Response {
	beast::http::status status;
	std::unordered_map<std::string, std::string> headers;
	std::optional<std::string> error_message;
	std::optional<boost::json::value> json;
	std::optional<std::filesystem::path> file;
	std::optional<std::string> body;
};
using Headers = std::unordered_map<std::string, std::string>;

void generatePasswordHashHashBase64(char* password_hash_hash_base64, size_t password_hash_hash_base64_len, const char* password_hash_base64, size_t password_hash_base64_len) {
	// hash of password hash in base64 is stored in DB
	unsigned char password_hash_hash[crypto_generichash_BYTES];
	crypto_generichash(
		password_hash_hash, crypto_generichash_BYTES,
		reinterpret_cast<const unsigned char*>(password_hash_base64), password_hash_base64_len,
					   NULL, 0
	);
	sodium_bin2base64(
		password_hash_hash_base64, password_hash_hash_base64_len,
		password_hash_hash, sizeof password_hash_hash,
		sodium_base64_VARIANT_URLSAFE
	);
}

template<class Map>
std::string generateKeyBase64(const Map& map) {
	// _NO_PADDING variant is used because the key is not expected to be converted back into binary
	char key_base64[sodium_base64_ENCODED_LEN(128/8, sodium_base64_VARIANT_URLSAFE_NO_PADDING)];
	do {
		unsigned char key_bytes[128/8];
		randombytes_buf(key_bytes, 128/8);
		sodium_bin2base64(
			key_base64, sizeof key_base64,
			key_bytes, 128/8,
			sodium_base64_VARIANT_URLSAFE_NO_PADDING
		);
	} while (map.contains(key_base64)); // It's not impossible for it to clash...
	return key_base64;
}

template<typename StateType>
void getSaltBase64(StateType state, const std::string& username, char* salt_base64) {
	unsigned char salt[crypto_pwhash_SALTBYTES];
	std::optional<int> user_id = state->getIdFromUsername(username);
	if (user_id) {
		std::string intermediate_salt_base64 = state->getIntermediateSaltFromAccount(user_id.value());
		// std::string intermediate_salt_base64 = "";
		crypto_generichash(
			salt, sizeof salt,
			reinterpret_cast<const unsigned char*>(username.c_str()), username.length(),
			reinterpret_cast<const unsigned char*>(intermediate_salt_base64.c_str()), intermediate_salt_base64.length()
		);
	}
	else {
		std::cout << "Username " << username << " not found. Generating fake salt." << std::endl;
		crypto_generichash(
			salt, sizeof salt,
			reinterpret_cast<const unsigned char*>(username.c_str()), username.length(),
			state->secret()->as_binary(), state->secret()->bytes
		);
	}
	sodium_bin2base64(
		salt_base64, sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE),
		salt, crypto_pwhash_SALTBYTES,
		sodium_base64_VARIANT_URLSAFE
	);
}

const std::string_view getMimeType(const std::string& path) {
	using beast::iequals;
	std::string_view ext = [&path] {
		auto const pos = path.rfind(".");
		if(pos == std::string_view::npos)
			return std::string_view{};
		return std::string_view(path).substr(pos);
	}();
	if(iequals(ext, ".aac"))  return "audio/aac";
	if(iequals(ext, ".flac")) return "audio/flac";
	if(iequals(ext, ".mid"))  return "audio/midi";
	if(iequals(ext, ".midi")) return "audio/midi";
	if(iequals(ext, ".mp3"))  return "audio/mpeg";
	if(iequals(ext, ".oga"))  return "audio/ogg";
	if(iequals(ext, ".ogx"))  return "audio/ogg";
	if(iequals(ext, ".opus")) return "audio/ogg";
	if(iequals(ext, ".js"))   return "application/javascript";
	if(iequals(ext, ".json")) return "application/json";
	if(iequals(ext, ".xml"))  return "application/xml";
	if(iequals(ext, ".swf"))  return "application/x-shockwave-flash";
	if(iequals(ext, ".avif")) return "image/avif";
	if(iequals(ext, ".bmp"))  return "image/bmp";
	if(iequals(ext, ".gif"))  return "image/gif";
	if(iequals(ext, ".heic")) return "image/heic";
	if(iequals(ext, ".heics"))return "image/heic";
	if(iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
	if(iequals(ext, ".jpe"))  return "image/jpeg";
	if(iequals(ext, ".jpeg")) return "image/jpeg";
	if(iequals(ext, ".jpg"))  return "image/jpeg";
	if(iequals(ext, ".jxl"))  return "image/jxl";
	if(iequals(ext, ".png"))  return "image/png";
	if(iequals(ext, ".tiff")) return "image/tiff";
	if(iequals(ext, ".tif"))  return "image/tiff";
	if(iequals(ext, ".svg"))  return "image/svg+xml";
	if(iequals(ext, ".svgz")) return "image/svg+xml";
	if(iequals(ext, ".webp")) return "image/webp";
	if(iequals(ext, ".htm"))  return "text/html";
	if(iequals(ext, ".html")) return "text/html";
	if(iequals(ext, ".php"))  return "text/html";
	if(iequals(ext, ".css"))  return "text/css";
	if(iequals(ext, ".txt"))  return "text/plain";
	if(iequals(ext, ".flv"))  return "video/x-flv";
	if(iequals(ext, ".mp4"))  return "video/mp4";
	if(iequals(ext, ".webm"))  return "video/webm";
	return "application/text";
}
template<typename BodyType>
http::response<BodyType> buildResponse(FuzeHttp::Response basic_response, const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& req);

template<>
http::response<http::string_body> buildResponse(FuzeHttp::Response basic_response, const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& req) {

	http::response<http::string_body> res{basic_response.status, req.version()};
	// res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
	for (auto& header : basic_response.headers)
		res.set(header.first, header.second);
	if (basic_response.error_message)
		res.set("message", basic_response.error_message.value());
	if (basic_response.json) {
		res.set(http::field::content_type, "application/json");
		res.body() = boost::json::serialize(basic_response.json.value());
	}
	else if (basic_response.error_message)
		res.body() = basic_response.error_message.value();
	else if (basic_response.body)
		res.body() = basic_response.body.value();
	res.keep_alive(req.keep_alive());
	res.prepare_payload();
	return res;
}

template<>
http::response<http::file_body> buildResponse(FuzeHttp::Response basic_response, const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& req) {
	std::cout << "Attempting to open " << basic_response.file.value() << std::endl;
	// Attempt to open the file
	beast::error_code ec;
	http::file_body::value_type body;
	body.open(basic_response.file.value().c_str(), beast::file_mode::scan, ec);

	if (ec) // Handle an unknown error
		throw std::runtime_error("Unknown error when attempting to open file");

	// Cache the size since we need it after the move
	const uint64_t size = body.size();

	http::response<http::file_body> res{
		std::piecewise_construct,
		std::make_tuple(std::move(body)),
		std::make_tuple(http::status::ok, req.version())
	};
	for (auto& header : basic_response.headers)
		res.set(header.first, header.second);
	// res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
	res.set(http::field::content_type, getMimeType(basic_response.file.value().string()));
	res.content_length(size);
	res.keep_alive(req.keep_alive());
	return res;
}

template<>
http::response<http::empty_body> buildResponse(FuzeHttp::Response basic_response, const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& req) {

	http::response<http::empty_body> res{basic_response.status, req.version()};
	// res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
	for (auto& header : basic_response.headers)
		res.set(header.first, header.second);
	if (basic_response.error_message)
		res.set("message", basic_response.error_message.value());
	res.keep_alive(req.keep_alive());
	res.prepare_payload();
	return res;
}
} // namespace FuzeHttp
