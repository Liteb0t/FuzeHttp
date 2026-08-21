module;
#include <variant>
export module FuzeHttp.ValidatedOrError;

export namespace FuzeHttp {
template<class T>
struct ValidatedOrError {
    std::variant<T, std::string> value;

    bool validated() const { return std::holds_alternative<T>(value); }
    T& unwrap() { return std::get<T>(value); }
    const std::string& error() const { return std::get<std::string>(value); }
};
}
