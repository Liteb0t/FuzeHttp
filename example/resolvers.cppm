module;
#include "shared_state.hpp"
#include <expected>
export module FuzeHttp.Example.Resolvers;
import FuzeHttp.Resolver;

export namespace FuzeHttp {
struct ObjectResolver : Resolver<shared_state*, std::shared_ptr<TestObject>, int> {
	std::expected<std::any, FuzeHttp::Response> fetch(shared_state* state, int key) const override {
		if (state->objects.contains(key))
			return state->objects.at(key);
		else
			return std::unexpected(Response{.status=http::status::not_found, .error_message=std::format("Could not find object at {}", key)});
	}
};
struct ChildObjectResolver : Resolver<shared_state*, TestChildObject*, int, std::shared_ptr<TestObject>> {
	std::expected<std::any, FuzeHttp::Response> fetch(shared_state* state, int key, std::shared_ptr<TestObject> test_object) const override {
		if (test_object->child_object_id == key)
			return state->child_objects.at(key).get();
		else
			return std::unexpected(Response{.status=http::status::not_found, .error_message=std::format("Non matching child object {}", key)});
	}
};
}
