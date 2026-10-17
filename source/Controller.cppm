// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include "beast.hpp"
// #include "Request.hpp"
// #include "ViewPath.hpp"
#include <unordered_set>
// #include <iostream>
#include <print>
export module FuzeHttp.Controller;
export import FuzeHttp.Client;
import FuzeHttp.Core;
import FuzeHttp.Request;
import FuzeHttp.Response;
import FuzeHttp.ViewPath;

export namespace FuzeHttp {
template<typename StateType>
class Controller {
	class EasyPatternAdder;
public:
	template<typename... Types>
	void addPattern(http::verb req_method, typename MakeFuncPtr<StateType, typename GetHandlerArgs<TypeList<Types...>, ToHandlerArg>::type>::type view, Types... args) {
		all_views.emplace(id_counter);
		views.emplace(id_counter, std::make_unique<ViewPath<StateType, Types...>>(req_method, view, std::move(args)...));
		// std::cout << "views[" << id_counter << "] length: " << views.at(id_counter)->path.size() << std::endl;
		id_counter++;
	}

	Response matchPathAndExecute(StateType state, Request req) {
		if (!req.pathName().starts_with('/'))
			return Response{.status = http::status::bad_request};
		// for (int id : all_views)
		// 	views.at(id)->clearResolvedObject();
		std::string_view kebab_path_name = req.pathName(); // first and last '/' removed.

		std::println("[Controller] {} {}", std::string(req.message.method_string()), req.pathName());
		if (kebab_path_name.ends_with('/'))
			kebab_path_name = kebab_path_name.substr(0, kebab_path_name.length() - 1);

		std::unordered_set<int> matched_views = all_views;
		std::vector<std::string_view> sections;
		std::string_view section;
		size_t section_index;
		size_t location_start_bound = 0;
		size_t location_end_bound;
		for (section_index = 0; location_start_bound < kebab_path_name.length(); section_index++) {
			location_start_bound++;
			location_end_bound = req.pathName().find('/', location_start_bound);
			if (location_end_bound == std::string_view::npos)
				section = kebab_path_name.substr(location_start_bound);
			else
				section = kebab_path_name.substr(location_start_bound, location_end_bound - location_start_bound);
			std::erase_if(matched_views, [this, &req, &section, section_index](const int view_id){
				return this->views.at(view_id)->attemptPathMatch(req.message.method(), section, section_index) == false;
			});
			sections.push_back(section);
			if (matched_views.size() == 0)
				break;
			else {
				location_start_bound = location_end_bound;
			}
		}
		int id_of_view_to_keep;
		// Remove matches for URLs shorter than the pattern
		std::erase_if(matched_views, [this, section_index](const int view_id){
			return this->views.at(view_id)->getPathSize() - this->views.at(view_id)->is_wild > section_index;
		});
		if (matched_views.size() > 1) {
			int last_path_length = 1000000000;
			// std::println("Multiple views matched");
			// Finds wildcard path with least number of segments, or any path that's absolute
			for (int view_id : matched_views) {
				if (this->views.at(view_id)->is_wild) {
					if (this->views.at(view_id)->getPathSize() < last_path_length) {
						last_path_length = this->views.at(view_id)->getPathSize();
						id_of_view_to_keep = view_id;
					}
				}
				else {
					id_of_view_to_keep = view_id;
					break;
				}
			}
			// std::erase_if(matched_views, [this](const int view_id){
			// 	return this->views.at(view_id)->is_wild;
			// });
		}
		else if (matched_views.size() == 1)
			id_of_view_to_keep = *matched_views.begin();
		else {
			// std::cout << "No patterns were matched to req.pathName() " << req.pathName() << std::endl;
			return FuzeHttp::Response{.status = http::status::not_found};
		}
		std::unique_ptr<TemporarySecretary<StateType>> temporary_secretary = views.at(id_of_view_to_keep)->createTemporarySecretary();
		for (int i = 0; i < sections.size(); ++i) {
			if (auto resolve_response = temporary_secretary->resolve(sections[i], i, state, req.client());
				!resolve_response)
				return resolve_response.error();
		}
		return temporary_secretary->executeView(state, req);
		// else if (req.method() == http::verb::get) {
		// 	return FuzeHttp::Response{.status = http::status::ok, .file = std::filesystem::canonical(req.pathName().substr(1), state->document_root)};
		// }
	}
	EasyPatternAdder addPatterns() { return EasyPatternAdder(this); }
private:
	class EasyPatternAdder {
	public:
		EasyPatternAdder(Controller* controller) : controller(controller) {}

		template<typename... Types>
		EasyPatternAdder& operator()(http::verb req_method, typename MakeFuncPtr<StateType, typename GetHandlerArgs<TypeList<Types...>, ToHandlerArg>::type>::type view, Types... args) {
			controller->addPattern(req_method, view, std::move(args)...);
			return *this;
		}
	private:
		Controller* controller;
	};
	// StateType state;
	std::unordered_set<int> all_views;
	std::unordered_map<int, std::unique_ptr<Path<StateType>>> views;
	int id_counter = 0;
}; // class Controller
} // namespace FuzeHttp
