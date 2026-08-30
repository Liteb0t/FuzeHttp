#pragma once
#include "beast.hpp"
#include "Request.hpp"
#include "ViewPath.hpp"
#include <unordered_set>
#include <iostream>
#include <print>
import FuzeHttp.Core;

namespace FuzeHttp {
template<typename StateType>
class Controller {
public:
	template</* template<typename...> class RequiresT, class... RequiresArgs, */typename... Types>
	constexpr void addPattern(http::verb req_method, typename MakeFuncPtr<StateType, typename Filter<TypeList<Types...>, IsViewArg>::type>::type view,/* Requires<RequiresArgs...> options = {}, */Types... args) {
	// constexpr void addPattern(http::verb req_method, View<> view, Options options, Types... args) {
		// ViewPath<Types...> vp(view, std::move(args)...);
		all_views.emplace(id_counter);
		views.emplace(id_counter, new ViewPath<StateType, /*RequiresArgs..., */Types...>(req_method, view, std::move(args)...));
		// std::cout << "views[" << id_counter << "] length: " << views.at(id_counter)->path.size() << std::endl;
		id_counter++;
	}

	Response matchPathAndExecute(StateType state, Request& req) {
		if (!req.target().starts_with('/'))
			return Response{.status = http::status::bad_request};

		std::string decoded_url = FuzeHttp::getDecodedURL(req.target());
		std::string_view path_name = FuzeHttp::getPathName(decoded_url);
		std::println("[Controller] {} {}", std::string(req.method_string()), path_name);
		if (path_name.ends_with('/'))
			path_name = path_name.substr(0, path_name.length() - 1);

		std::unordered_set<int> matched_views = all_views;
		std::string_view section;
		size_t section_index;
		size_t location_start_bound = 0;
		size_t location_end_bound;
		for (section_index = 0; location_start_bound < path_name.size(); section_index++) {
			location_start_bound++;
			location_end_bound = path_name.find('/', location_start_bound);
			if (location_end_bound == std::string_view::npos)
				section = path_name.substr(location_start_bound);
			else
				section = path_name.substr(location_start_bound, location_end_bound - location_start_bound);
			std::erase_if(matched_views, [this, &req, &section, section_index](const int view_id){
				return this->views.at(view_id)->attemptPathMatch(req.method(), section, section_index) == false;
			});
			// std::cout << '.' << std::endl;
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
			// std::cout << "No patterns were matched to path_name " << path_name << std::endl;
			return FuzeHttp::Response{.status = http::status::not_found};
		}
			return views.at(id_of_view_to_keep)->executeView(state, req);
		// else if (req.method() == http::verb::get) {
		// 	return FuzeHttp::Response{.status = http::status::ok, .file = std::filesystem::canonical(path_name.substr(1), state->document_root)};
		// }
	}
private:
	// StateType state;
	std::unordered_set<int> all_views;
	std::unordered_map<int, Path<StateType>*> views;
	int id_counter = 0;
}; // class Controller
} // namespace FuzeHttp
