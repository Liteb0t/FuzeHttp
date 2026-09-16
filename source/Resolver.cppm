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
	virtual std::expected<std::any, FuzeHttp::Response> resolve(StateType state, const std::any& parent_object, std::string_view section, const std::optional<Client>& client) const = 0;
};

template<class StateType, class Object, typename Key, class ParentObject = void>
struct Resolver : ResolverBase<StateType> {
	using object_type = Object;
	std::expected<Key, std::string> parse(std::string_view section) const {
		return parseSection<Key>(section); // TODO allow user to define his/her own parser
	}
	// TODO ensure returned success value matches type Object (to avoid bad any_cast exception)
	virtual std::expected<std::any, FuzeHttp::Response> fetch(StateType state, Key key, const std::optional<Client>& client) const = 0;
	std::expected<std::any, FuzeHttp::Response> resolve(StateType state, const std::any& parent_object, std::string_view section, const std::optional<Client>& client) const final {
		if (auto key = parse(section); !key)
			return std::unexpected(FuzeHttp::Response{.status=http::status::bad_request, .error_message=key.error()});
		else
			return fetch(state, key.value(), client);
	}
};

template<class StateType, class Object, typename Key, class ParentObject>
requires (!std::is_void_v<ParentObject>)
struct Resolver<StateType, Object, Key, ParentObject> : ResolverBase<StateType> {
	using object_type = Object;
	std::expected<Key, std::string> parse(std::string_view section) const {
		return parseSection<Key>(section); // TODO allow user to define his/her own parser
	}
	virtual std::expected<std::any, FuzeHttp::Response> fetch(StateType state, Key key, ParentObject parent_object, const std::optional<Client>& client) const = 0;
	std::expected<std::any, FuzeHttp::Response> resolve(StateType state, const std::any& parent_object, std::string_view section, const std::optional<Client>& client) const final {
		if (auto key = parse(section); !key)
			return std::unexpected(FuzeHttp::Response{.status=http::status::bad_request, .error_message=key.error()});
		else {
			// Pointer overload: returns nullptr on mismatch instead of throwing.
			const ParentObject* parent_value = std::any_cast<ParentObject>(&parent_object);
			if (!parent_value) {
				return std::unexpected(FuzeHttp::Response{
					.status = http::status::internal_server_error,
					.error_message = std::format("ParentObject not resolved for section `{}`", section)
				});
			}
			else
				return fetch(state, key.value(), *parent_value, client);
		}
	}
};
}
