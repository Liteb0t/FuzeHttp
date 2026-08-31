module;
#include <cstring>
#include <string>
#include <sodium.h>
#include <bits/unique_ptr.h>
export module FuzeHttp.Secret;

export namespace FuzeHttp {
class Secret {
public:
	Secret(const char* secret_pointer) : bytes(std::strlen(secret_pointer)), binary(std::make_unique<unsigned char[]>(bytes)) {
        std::memcpy(binary.get(), secret_pointer, bytes);
		base64.resize(sodium_base64_ENCODED_LEN(sizeof secret_pointer, sodium_base64_VARIANT_URLSAFE));
		sodium_bin2base64(
			base64.data(), base64.size(),
			binary.get(), sizeof secret_pointer,
			sodium_base64_VARIANT_URLSAFE
		);
		// std::string as_base64 = secret_base64_a;
		hex.resize(bytes * 2 + 1);
		sodium_bin2hex(
			hex.data(), hex.size(),
			binary.get(), bytes
		);
	}
    ~Secret() {
        if (bytes) sodium_memzero(binary.get(), bytes); // wipe secret from memory
    }
	const size_t bytes;
	const unsigned char* as_binary() const { return binary.get(); }
	const std::string& as_base64() const { return base64; }
	const std::string& as_hex() const { return hex; }
private:
    std::unique_ptr<unsigned char[]> binary;
	std::string hex;
	std::string base64;
};
}
