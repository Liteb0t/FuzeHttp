// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
#pragma once
#include "beast.hpp"

namespace FuzeHttp {
typedef const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& Request;
}
