module;
#include "shared_state.hpp"
#include <expected>
export module FuzeHttp.Example.Resolvers;
import FuzeHttp.Resolver;

export namespace FuzeHttp {
struct ObjectResolver : Resolver<shared_state*, TestObject*, int> {
	std::expected<std::any, FuzeHttp::Response> fetch(shared_state* state, int key) const override {
		if (state->objects.contains(key))
			return &(state->objects.at(key));
		else
			return Response{.status=http::status::not_found, .error_message=std::format("Could not find object at {}", key)};
	}
};
// struct ChildObjectResolver : Resolver<shared_state*, TestObject*, int> {
// 	std::expected<std::any, FuzeHttp::Response> fetch(shared_state* state, int key) const override {
// 		if (state->objects.contains(key))
// 			return &(state->objects.at(key));
// 		else
// 			return Response{.status=http::status::not_found, .error_message=std::format("Could not find object at {}", key)};
// 	}
// };
}
