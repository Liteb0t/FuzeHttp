module;
#include <chrono>
#include <string>
#include <optional>
export module FuzeHttp.Client;

export namespace FuzeHttp {
struct Account {
	inline static const int PUBLIC = 0;
	inline static const size_t MAX_USERNAME = 32;
	inline static const size_t MIN_PASSWORD = 3;
	const int id;
	std::optional<int> client_id;
	std::string username;
};

struct Client {
	int id;
	std::optional<int> account_id;
	// const std::string session_id;
};

struct Session {
	const int id;
	const int client_id;
	// const std::string key;
	const std::chrono::time_point<std::chrono::system_clock> created_at;
};
} // namespace FuzeHttp
