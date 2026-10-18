// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include "beast.hpp"
#include <boost/json.hpp>
#include <optional>
#include <unordered_map>
#include <print>
#include <ranges>
#include <string_view>
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
	const std::unordered_map<std::string_view, std::string_view>& searchParameters() {
		if (!_search_parameters)
			_search_parameters = getSearchParameters(_decoded_target);
		return _search_parameters.value();
	}
private:
	const std::optional<Client> _client;
	std::string _decoded_target;
	const std::string_view _pathname;
	std::optional<boost::json::object> _json;
	std::optional<std::unordered_map<std::string_view, std::string_view>> _search_parameters;
	std::unordered_map<std::string_view, std::string_view> getSearchParameters(const std::string& decoded_url) const {
		std::unordered_map<std::string_view, std::string_view> search_parameters;
		size_t q_index = decoded_url.rfind('?');
		if (q_index == std::string::npos)
			return search_parameters;
		const std::string_view search = std::string_view(decoded_url).substr(decoded_url.rfind('?')+1);
		std::println("Search: \"{}\"", search);
		for (const auto parameter : std::views::split(search, '&')) {
			std::println("Parameter: {}", parameter);
			const std::string_view parameter_view = std::string_view(parameter);
			size_t equals_index = parameter_view.find('=');
			if (equals_index != std::string::npos) {
				const std::string_view key = parameter_view.substr(0, equals_index);
				const std::string_view value = parameter_view.substr(equals_index+1);
				std::println("{} :: {}", key, value);
				search_parameters.emplace(key, value);
			}
		}
		return search_parameters;
	}
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
