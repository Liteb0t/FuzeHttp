// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include "beast.hpp"
#include <boost/json.hpp>
#include <optional>
export module FuzeHttp.Request;
import FuzeHttp.Client;

export namespace FuzeHttp {
class _Request {
public:
	using MessageType = const http::request<http::string_body, http::basic_fields<std::allocator<char>>>;
	_Request(MessageType&& message, std::optional<Client> client)
			: message(std::move(message)),
			_client(client),
			_decoded_target(getDecodedURL(message.target())),
			_pathname(getPathName(_decoded_target)) {
	}
	MessageType message; // from <boost/beast/http/message.hpp>
	const std::optional<Client>& client() const { return this->_client; }
	const std::string_view pathName() const { return this->_pathname; }
	boost::json::object& json() {
		if (!_json)
			_json = boost::json::parse(message.body()).as_object();
		return _json.value();
	}
private:
	const std::optional<Client> _client;
	std::string _decoded_target;
	const std::string_view _pathname;
	std::optional<boost::json::object> _json;
	static char fromHex(char ch) {
		return std::isdigit(ch) ? ch - '0' : std::tolower(ch) - 'a' + 10;
	}
	static std::string getDecodedURL(boost::string_view raw_URL) {
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
	static const std::string_view getPathName(const std::string& source_URL) {
		// path_name excludes URL parameters (stuff after '?')
		// removes trailing / but leaves first /
		std::string_view path_name = source_URL;
		int decoded_url_questionmark_index = source_URL.rfind('?');
		if (decoded_url_questionmark_index != std::string::npos)
			path_name = path_name.substr(0, decoded_url_questionmark_index);
		return path_name;
	}
};
using Request = _Request&;
} // export namespace FuzeHttp
