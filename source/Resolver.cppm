module;
#include "beast.hpp"
#include <any>
#include <charconv>
#include <expected>
#include <string>
#include <string_view>
#include <format>
export module FuzeHttp.Resolver;
export import FuzeHttp.Core;

export namespace FuzeHttp {
template<typename Key>
std::expected<Key, std::string> parseSection(std::string_view section);

template<>
inline std::expected<std::string, std::string> parseSection(std::string_view section) { return std::string(section); }

template<>
inline std::expected<int, std::string> parseSection(std::string_view section) {
	int value;
	std::from_chars_result res = std::from_chars(section.data(), section.data() + section.size(), value);
	if (res.ec == std::errc())
		return value;
	else
		return std::unexpected(std::format("Failed to parse int from section `{}`", section));
}


template<class StateType>
struct ResolverBase {
	virtual ~ResolverBase() = default;
	virtual std::expected<std::any, FuzeHttp::Response> resolve(StateType state, std::string_view section) const = 0;
};

template<class StateType, class Object, typename Key = std::string>
struct Resolver : ResolverBase<StateType> {
	using object_type = Object;
	std::expected<Key, std::string> parse(std::string_view section) const {
		return parseSection<Key>(section); // TODO allow user to define his/her own parser
	}
	virtual std::expected<std::any, FuzeHttp::Response> fetch(StateType state, Key key) const = 0;
	std::expected<std::any, FuzeHttp::Response> resolve(StateType state, std::string_view section) const final {
		if (auto key = parse(section); !key)
			return FuzeHttp::Response{.status=http::status::bad_request, .error_message=key.error()};
		else {
			if (auto result = fetch(state, key.value()); !result)
				return std::unexpected(result.error());
			else
				return std::any_cast<Object>(result.value());
		}
	}
};
}
