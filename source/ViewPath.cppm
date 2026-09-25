// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
// #pragma once
module;
#include "beast.hpp"
#include <expected>
#include <filesystem>
#include <print>
import FuzeHttp.Core;
import FuzeHttp.Utils;
import FuzeHttp.PermissionObject;
import FuzeHttp.Resolver;
export module FuzeHttp.ViewPath;

export namespace FuzeHttp {

typedef const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& Request;

// https://stackoverflow.com/a/79894118/18658154
// Type Filtering Logic

template<typename T>
struct IsViewArg : std::disjunction<std::is_same<T, int>, std::is_same<T, std::string>, std::is_same<T, Client>> {};

template<typename... Ts> struct TypeList {};

template<typename T>
struct ObjectTypeOf {
	using type = void;
};
template<typename T>
requires requires { typename T::object_type; }
struct ObjectTypeOf<T> {
	using type = typename T::object_type;
};
template<typename T>
struct ToHandlerArg {
	using type = std::conditional_t<
		std::disjunction_v<std::is_same<T, int>, std::is_same<T, std::string>, std::is_same<T, Client>>,
		T,
		typename ObjectTypeOf<T>::type>;
};

// template<typename T>
// struct IsPathArg : std::disjunction<IsViewArg<T>, std::is_same<T, const char*>> {};

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

template<typename In, template<typename> class Pred, typename Out = TypeList<>>
struct GetHandlerArgs;

template<template<typename> class Pred, typename... Out>
struct GetHandlerArgs<TypeList<>, Pred, TypeList<Out...>> { using type = TypeList<Out...>; };

template<typename T, typename... Rest, template<typename> class Pred, typename... Out>
struct GetHandlerArgs<TypeList<T, Rest...>, Pred, TypeList<Out...>> {
	using type = typename std::conditional_t<std::is_same_v<typename Pred<T>::type, void>,
		GetHandlerArgs<TypeList<Rest...>, Pred, TypeList<Out...>>,
		GetHandlerArgs<TypeList<Rest...>, Pred, TypeList<Out..., typename Pred<T>::type>>>::type;
};

template<typename StateType, typename T> struct MakeFuncPtr;
template<typename StateType, typename... Args>
struct MakeFuncPtr<StateType, TypeList<Args...>> { using type = Response(*)(StateType, const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& req, Args...); };


// template<typename StateType, typename T> struct MakeHandlerPtr;
// template<typename StateType, typename... Args>
// struct MakeFuncPtr<StateType, TypeList<Args...>> { using type = Response(*)(StateType, const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& req, Args...); };

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
class TemporarySecretary {
public:
	virtual ~TemporarySecretary() = default;
	virtual Response executeView(StateType state, Request& req) = 0;
	virtual std::expected<void, Response> resolve(std::string_view section, size_t index, StateType state, const std::optional<Client>& client) = 0;
};

template<typename StateType, size_t NumberOfHandlerArgs, class FuncPtr, class ArgTuple>
class IDontNeedNoBellyDancer : public TemporarySecretary<StateType> { // invoked during URL pattern matching
	enum VARIANT : int { CHARS = 0, INT, STRING, CLIENT, RESOLVER};
	using ArgVariant = std::variant<const char*, int, std::string, Client, std::shared_ptr<ResolverBase<StateType>>>;
public:
	IDontNeedNoBellyDancer(FuncPtr view_func, ArgTuple view_args, const std::span<const int> pattern_position_to_view_arg_index, const std::span<const ArgVariant> all_args, int path_starts_at) : view_func(view_func), view_args(view_args), pattern_position_to_view_arg_index(pattern_position_to_view_arg_index), all_args(all_args), path_starts_at(path_starts_at) {}
	Response executeView(StateType state, Request& req) override {
		std::optional<int> set_session_for_client_id;
		if (std::tuple_size<ArgTuple>::value > 0 && this->all_args[0].index() == 3) { // There is a Client{} parameter in the view
			std::optional<Client> client = state->getClientIfExists(req);
			if (!client) {
				client = state->createClient(); // Create anonymous client, because accounts are assigned a client on login
				set_session_for_client_id = client.value().id;
			}
			std::println("client ID is: {}", client.value().id);
			this->setArg(0, client.value());
		}
		std::println();
		Response res = std::apply(view_func, std::tuple_cat(std::tie(state, req), /* extra_args */ view_args));
		if (set_session_for_client_id) {
			std::string session_id_base64 = state->createSession(set_session_for_client_id.value());
			res.headers.insert({"Set-Cookie", formatCookie(session_id_base64)});
		}
		if (res.file) { // cache controle
			if (auto early_response = setCacheControl(state, res, req))
				return early_response.value();
		}
		return res;
	}
	// call only AFTER asserting attemptPathMatch(index ...) == true
	std::expected<void, Response> resolve(std::string_view section, size_t index, StateType state, const std::optional<Client>& client) override {
		index += this->path_starts_at;
		if (index >= this->all_args.size())
			return {}; //std::unexpected(Response{.status=http::status::bad_request, .error_message="index >= this->all_args.size()."});
		const ArgVariant& vari = this->all_args[index];

		if (vari.index() == VARIANT::INT) {
			int value;
			std::from_chars_result res = std::from_chars(section.data(), section.data() + section.size(), value);
			if (res.ec == std::errc()) {
				this->setArg(pattern_position_to_view_arg_index[index], value);
				return {};
			}
			else {
				return std::unexpected(Response{.status=http::status::bad_request, .error_message=std::format("{} is not a valid integer.", section)});
			}
		}
		else if (vari.index() == VARIANT::STRING) {
			this->setArg(pattern_position_to_view_arg_index[index], section);
			return {};
		}
		else if (vari.index() == VARIANT::RESOLVER) {
			auto resolver = std::get<std::shared_ptr<ResolverBase<StateType>>>(vari);
			std::expected<std::any, Response> resolved = resolver->resolve(state, this->previous_resolved_object, section, client);
			if (!resolved)
				return std::unexpected(resolved.error());
			else {
				this->previous_resolved_object = resolved.value();
				this->setArg(pattern_position_to_view_arg_index[index], resolved.value());
			}
		}
		return {};
	}
private:
	// https://stackoverflow.com/a/28440573/18658154
	template<std::size_t I = 0, typename T>
	inline typename std::enable_if<I == NumberOfHandlerArgs, void>::type
	setArg(int, T) { }

	template<std::size_t I = 0, typename T>
	inline typename std::enable_if<I < NumberOfHandlerArgs, void>::type
	setArg(int index, T value) {
		if (index == 0) {
			auto& entry = std::get<I>(this->view_args);
			if constexpr (std::is_same_v<std::remove_cvref_t<T> /* not sure if remove cv/ref is necessary but Claude suggested it*/, std::any>) {
				if (auto* obj = std::any_cast<std::remove_reference_t<decltype(entry)>>(&value))
					entry = *obj;
				else
					throw std::runtime_error(std::format("std::any_cast failed for index {}", I));
			}
			// Shoutouts to David G https://stackoverflow.com/a/79897965/18658154
			else if constexpr (requires{ entry = value; }) {
				entry = value;
			}
		}
		setArg<I + 1, T>(index-1, value);
	}
	static std::optional<Response> setCacheControl(StateType state, Response& res, Request& req) {
		if (std::filesystem::is_directory(res.file.value()))
			res.file = res.file.value() / "index.html";
		std::string target = std::filesystem::proximate(res.file.value(), state->getDocumentRoot()).string();
		// std::println("Proximate target (pre):  {}", target);

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
				return Response{.status=http::status::not_modified};
			}
		}
		res.headers.emplace("ETag", etag);
		std::println("Proximate target (post):  {}", target);
		return {};
	}
	void clearResolvedObject() { previous_resolved_object = std::any{}; }
	std::any previous_resolved_object; // from Resolver
	FuncPtr view_func;
	ArgTuple view_args;
	const std::span<const int> pattern_position_to_view_arg_index;
	const std::span<const ArgVariant>(all_args);
	int path_starts_at;
};

template<typename StateType>
class Path {
public:
	virtual ~Path() = default;
	virtual size_t getPathSize() const = 0;
	// virtual Response executeView(StateType state, Request& req) = 0;
	virtual bool attemptPathMatch(http::verb req_method, std::string_view section, size_t index) const = 0;
	virtual std::unique_ptr<TemporarySecretary<StateType>> createTemporarySecretary() const = 0;
	// virtual std::expected<void, Response> resolveResolverIfTheArgVariantThingForThisIndexIsResolverBase(std::string_view section, size_t index, StateType state, const std::optional<Client>& client) = 0;
	bool is_wild = false;
};

template<typename StateType, class... AllArgs>
class ViewPath : public Path<StateType> {
	// using PathArgs = typename Filter<TypeList<AllArgs...>, IsPathArg>::type;
	using FilteredTypes = typename Filter<TypeList<AllArgs...>, IsViewArg>::type;
	using HandlerArgs = typename GetHandlerArgs<TypeList<AllArgs...>, ToHandlerArg>::type;
	// using FilteredTypes = typename Filter<TypeList<AllArgs...>, IsExtraArg>::type;
	using FuncPtr = typename MakeFuncPtr<StateType, HandlerArgs>::type;
	using ArgTuple = typename MakeArgTuple<HandlerArgs>::type;
	enum VARIANT : int { CHARS = 0, INT, STRING, CLIENT, RESOLVER};
	using ArgVariant = std::variant<const char*, int, std::string, Client, std::shared_ptr<ResolverBase<StateType>>>;
	// using ExtrasTuple = typename MakeArgTuple<FilteredExtraTypes>::type;

	// [AI glasnost] this section generated by Claude Sonnet 5
	template<typename T>
	static ArgVariant makeArgVariant(T&& value) {
		if constexpr (requires { typename std::remove_cvref_t<T>::object_type; }) {
			using Resolved = std::remove_cvref_t<T>;
			return ArgVariant{ std::make_shared<Resolved>(std::forward<T>(value)) };
		}
		else {
			return ArgVariant{ std::forward<T>(value) };
		}
	}
	// [AI glasnost] end AI-generated section
public:
	ViewPath(http::verb req_method, FuncPtr v, AllArgs... args)
			: view_func_empty(v),
			req_method(req_method) {
		size_t arg_index, index;
		arg_index = index = 0;
		for (ArgVariant var : { makeArgVariant(std::forward<AllArgs>(args))... }) {
			this->all_args[index] = var;
			if (var.index() == 3) {
				this->path_starts_at++;
			}
			if (!std::holds_alternative<const char*>(var)) {
				this->pattern_position_to_view_arg_index[index] = arg_index++; // TODO make similar array for parent objects in resolvers
				// std::cout << "Arg is not a char array!" << std::endl;
			}
			else
				this->is_wild = std::get<const char*>(var)[0] == '*';
			index++;
		}
		// std::cout << "Final all_args length: " << this->all_args.size() << std::endl;
	}
	virtual std::unique_ptr<TemporarySecretary<StateType>> createTemporarySecretary() const override {
		return std::make_unique<IDontNeedNoBellyDancer<StateType, SizeOfT<HandlerArgs>::value, FuncPtr, ArgTuple>>(
			view_func_empty,
			view_args_empty,
			std::span<const int>(pattern_position_to_view_arg_index),
			std::span<const ArgVariant>(all_args),
			path_starts_at
		); // empty view_func and view_args are copied over
	}
	size_t getPathSize() const override {
		return this->all_args.size() - this->path_starts_at;
	}
	bool attemptPathMatch(http::verb req_method, std::string_view section, size_t index) const override {
		index += this->path_starts_at;
		// std::cout << "Path starts at " << this->path_starts_at << std::endl;
		if (req_method != this->req_method)
			return false;
		// std::println("Index: {} \tall_args: {}", index, this->all_args.size());
		if (this->is_wild && index >= this->all_args.size() - 1)
			return true;
		else if (index >= this->all_args.size()) {
			return false;
		}
		// std::cout << ", getting variant";
		const ArgVariant& vari = this->all_args[index];

		// std::print("Section: \"{}\"", section);
		if (vari.index() == VARIANT::CHARS) { // Not a view arg
			std::string str = std::string(std::get<const char*>(vari));
			return str == section;
		}
		else if (vari.index() == VARIANT::INT) { // Integer arg
			int value;
			std::from_chars_result res = std::from_chars(section.data(), section.data() + section.size(), value);
			if (res.ec == std::errc()) {
				return true;
			}
			else {
				return false;
			}
		}
		else if (vari.index() == VARIANT::STRING) { // String arg
			return true;
		}
		else if (vari.index() == VARIANT::RESOLVER) {
			return true;
		}
		else
			throw std::runtime_error(std::format("Variant {} is not a path arg", vari.index()));
	}

private:

	const FuncPtr view_func_empty;
	const ArgTuple view_args_empty;
	// ExtrasTuple extra_args;
	http::verb req_method;
	// std::vector<std::variant<Client, const char*, int, std::string>> path;
	std::array<ArgVariant, sizeof...(AllArgs)> all_args;
	int path_starts_at = 0;
	std::array<int, sizeof...(AllArgs)> pattern_position_to_view_arg_index; // maps arg Pattern position to View arg position
};
} // namespace FuzeHttp
