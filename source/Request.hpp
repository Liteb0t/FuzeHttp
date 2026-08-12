#pragma once
#include "beast.hpp"

namespace FuzeHttp {
typedef const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& Request;
}
