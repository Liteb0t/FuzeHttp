#pragma once
#include "beast.hpp"
#include <filesystem>
#include <print>
import FuzeHttp.Core;
import FuzeHttp.Utils;
import FuzeHttp.PermissionObject;

namespace FuzeHttp {

typedef const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& Request;

// https://stackoverflow.com/a/79894118/18658154
// Type Filtering Logic
template<typename... Ts> struct TypeList {};

template<typename T>
struct IsViewArg : std::disjunction<std::is_same<T, int>, std::is_same<T, std::string>, std::is_same<T, Client>> {};

template<typename T>
struct IsPathArg : std::disjunction<IsViewArg<T>, std::is_same<T, const char*>> {};

template<typename In, template<typename> class Pred, typename Out = TypeList<>>
struct Filter;

template<template<typename> class Pred, typename... Out>
struct Filter<TypeList<>, Pred, TypeList<Out...>> { using type = TypeList<Out...>; };

template<typename T, typename... Rest, template<typename> class Pred, typename... Out>
struct Filter<TypeList<T, Rest...>, Pred, TypeList<Out...>> {
	using type = typename std::conditional_t<Pred<T>::value,
	Filter<TypeList<Rest...>, Pred, TypeList<Out..., T>>,
	Filter<TypeList<Rest...>, Pred, TypeList<Out...>>>::type;
};

template<typename StateType, typename T> struct MakeFuncPtr;
template<typename StateType, typename... Args>
struct MakeFuncPtr<StateType, TypeList<Args...>> { using type = Response(*)(StateType, const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& req, Args...); };

template<typename T> struct MakeArgTuple;
template<typename... Args>
struct MakeArgTuple<TypeList<Args...>> { using type = std::tuple<Args...>; };

// https://stackoverflow.com/a/60882359/18658154
template <typename H>
struct SizeOfT;

template <template <typename...> class TL, typename... Ts>
struct SizeOfT <TL<Ts...>> {
	constexpr static auto value = sizeof...(Ts);
};

template<typename StateType>
class Path {
public:
	virtual size_t getPathSize() const = 0;
	virtual Response executeView(StateType state, Request& req) = 0;
	virtual bool attemptPathMatch(http::verb req_method, std::string_view section, size_t index) = 0;
	bool is_wild = false;
};

template<typename StateType, class... AllArgs>
class ViewPath : public Path<StateType> {
	// using PathArgs = typename Filter<TypeList<AllArgs...>, IsPathArg>::type;
	using FilteredTypes = typename Filter<TypeList<AllArgs...>, IsViewArg>::type;
	// using FilteredTypes = typename Filter<TypeList<AllArgs...>, IsExtraArg>::type;
	using FuncPtr = typename MakeFuncPtr<StateType, FilteredTypes>::type;
	using ArgTuple = typename MakeArgTuple<FilteredTypes>::type;
	// using ExtrasTuple = typename MakeArgTuple<FilteredExtraTypes>::type;
public:
	constexpr ViewPath(http::verb req_method, FuncPtr v, AllArgs... args)
			: view_func(v),
			req_method(req_method) {
		size_t arg_index, index;
		arg_index = index = 0;
		// this->all_args = std::initializer_list<std::variant<const char*, int, std::string>>{ args... };
		// int all_args_i = 0;
		for (std::variant<const char*, int, std::string, Client> var : std::initializer_list<std::variant<const char*, int, std::string, Client>>{ args... }) {
			this->all_args[index] = var;
			if (var.index() == 3) {
				this->path_starts_at++;
			}
			if (!std::holds_alternative<const char*>(var)) {
				this->pattern_position_to_view_arg_index[index] = arg_index++;
				// std::cout << "Arg is not a char array!" << std::endl;
			}
			else
				this->is_wild = std::get<const char*>(var)[0] == '*';
			index++;
		}
		// std::cout << "Final all_args length: " << this->all_args.size() << std::endl;
	}
	Response executeView(StateType state, Request& req) override {
		std::optional<int> set_session_for_client_id;
		if (std::tuple_size<ArgTuple>::value > 0 && this->all_args[0].index() == 3) { // There is a Client{} parameter in the view
			std::optional<Client> client = state->getClientIfExists(req);
			if (!client) {
				client = state->createClient(); // Create anonymous client, because accounts are assigned a client on login
				set_session_for_client_id = client.value().id;
			}
			// std::cout << "client ID is: " << client.value().id << std::endl;
			this->setArg(0, client.value());
		}
		Response res = std::apply(view_func, std::tuple_cat(std::tie(state, req), /* extra_args */ view_args));
		if (set_session_for_client_id) {
			std::string session_id_base64 = state->createSession(set_session_for_client_id.value());
			res.headers.insert({"Set-Cookie", formatCookie(session_id_base64)});
		}
		if (res.file) { // cache controle
			if (std::filesystem::is_directory(res.file.value()))
				res.file = res.file.value() / "index.html";
			std::print("manifest_frontend_etags: ");
			for (const auto& target : state->manifest_frontend_etags)
				std::println("{} :: {}", target.first, target.second);

			std::println("Busted target to target:");
			for (const auto& target :state-> busted_target_to_target)
				std::println("{} :: {}", target.first, target.second);
			std::string target = std::filesystem::proximate(res.file.value(), state->getDocumentRoot()).string();
			std::println("Proximate target (pre):  {}", target);

			if (target.empty() || target.ends_with('/'))
				target += "index.html";
			// Is static asset
			if (auto it = state->busted_target_to_target.find(target); it != state->busted_target_to_target.end()) {
				target = it->second;
				res.file = state->getDocumentRoot() / target;
				res.headers.emplace("Cache-Control", "max-age=7750000, immutable");
			}
			// If path leads to target of .GENERATED file, add the filename extension
			else if (auto it = state->files_generated_from_templates.find(target); it != state->files_generated_from_templates.end()) {
				target = FuzeHttp::insertExtensionToFileName(*it, ".GENERATED");
				res.file = state->getDocumentRoot() / target;
				res.headers.emplace("Cache-Control", "no-cache");
			}
			std::println("[showMainPage] will serve {}", target);
			std::string etag;
			if (std::unordered_map<std::string /*target*/, std::string /*etag*/>::const_iterator it = state->manifest_frontend_etags.find(target); it != state->manifest_frontend_etags.end()) {
				std::println("Found manifest etag {}", it->second);
				etag = it->second;
			}
			else
				etag = state->frontend_etag;

			if (auto if_none_match_header = req.find("If-None-Match"); if_none_match_header != req.end()) {
				std::string if_none_match_header_value = if_none_match_header->value();
				if (etag == if_none_match_header_value) {
					return {.status=http::status::not_modified};
				}
			}
			res.headers.emplace("ETag", etag);
			std::println("Proximate target (post):  {}", target);
		}
		return res;
	}
	size_t getPathSize() const override {
		return this->all_args.size() - this->path_starts_at;
	}
	bool attemptPathMatch(http::verb req_method, std::string_view section, size_t index) override {
		index += this->path_starts_at;
		// std::cout << "Path starts at " << this->path_starts_at << std::endl;
		if (req_method != this->req_method)
			return false;
		// std::println("Index: {} \tall_args: {}", index, this->all_args.size());
		if (this->is_wild && index >= this->all_args.size() - 1)
			return true;
		else if (index >= this->all_args.size()) {
			// std::cout << "Index " << index << "Is greater than number of args " << this->all_args.size() << std::endl;
			return false;
		}
		// std::cout << ", getting variant";
		const std::variant<const char*, int, std::string, Client> vari = this->all_args[index];

		// std::cout << "Section: \"" << section << "\"";
		if (vari.index() == 0) { // Not a view arg
			std::string str = std::string(std::get<const char*>(vari));
			// std::cout << ", is const \"" << str << '"';
			return str == section;
		}
		else if (vari.index() == 1) { // Integer arg
			int value;
			std::from_chars_result res = std::from_chars(section.data(), section.data() + section.size(), value);
			if (res.ec == std::errc()) {
				// std::cout << ", found value " << value;
				this->setArg(pattern_position_to_view_arg_index[index], value);
				// std::cout << ", returning.";
				return true;
			}
			else {
				// if (res.ec == std::errc::invalid_argument)
					// std::cout << ", this is not a number.\n";
				// if (res.ec == std::errc::result_out_of_range)
					// std::cout << ", this number is larger than an int.\n";
				return false;
			}
		}
		else if (vari.index() == 2) { // String arg
			// std::cout << ", Is string \"" << section << '"';
			this->setArg(pattern_position_to_view_arg_index[index], section);
			// this->setArg<(size_t)0, Functor, int, pattern_position_to_view_arg_index[index]>(pattern_position_to_view_arg_index[index], Functor(), section);
			return true;
		}
		else
			throw std::runtime_error(std::format("Variant {} is not a path arg", vari.index()));
	}
private:
	// https://stackoverflow.com/a/28440573/18658154
	template<std::size_t I = 0, typename T>
	inline typename std::enable_if<I == SizeOfT<FilteredTypes>::value, void>::type
	setArg(int, T) { }

	template<std::size_t I = 0, typename T>
	inline typename std::enable_if<I < SizeOfT<FilteredTypes>::value, void>::type
	setArg(int index, T value) {
		if (index == 0) {
			// Shoutouts to David G https://stackoverflow.com/a/79897965/18658154
			if constexpr (auto& entry = std::get<I>(this->view_args); requires{ entry = value; }) {
				entry = value;
			}
		}
		setArg<I + 1, T>(index-1, value);
	}
	FuncPtr view_func;
	ArgTuple view_args;
	// ExtrasTuple extra_args;
	http::verb req_method;
	// std::vector<std::variant<Client, const char*, int, std::string>> path;
	std::array<std::variant<const char*, int, std::string, Client>, sizeof...(AllArgs)> all_args;
	int path_starts_at = 0;
	std::array<int, sizeof...(AllArgs)> pattern_position_to_view_arg_index; // maps arg Pattern position to View arg position
	// std::tuple<FilteredTypes> view_args;

	// TODO get filtered tuple at compile time
	/*
	// This helper returns a 1-element tuple if T is integral, otherwise an empty tuple.*
	template<typename T>
	auto wrap_if_integral(T&& val) const {
		if constexpr (std::is_integral_v<std::decay_t<T>>) {
			return std::make_tuple(std::forward<T>(val));
		} else {
			return std::tuple<>{};
		}
	}
	template<size_t... Is>
	void invoke_helper(std::index_sequence<Is...>) const {
	    // tuple_cat joins all the 1-element and 0-element tuples into one flat list
	    auto filtered_args = std::tuple_cat(wrap_if_integral(std::get<Is>(view_args))...);
	    std::apply(view_func, filtered_args);
	}
	*/
};
} // namespace FuzeHttp
