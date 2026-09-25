// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <boost/container/container_fwd.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include <boost/smart_ptr.hpp>
#include <print>
#include <string>
#include <type_traits>
export module FuzeHttp.ProgramOptions;

export namespace FuzeHttp {
template<typename T>
std::string valueAsString(const T& value);

template<typename T>
requires(requires(const T& val) {std::to_string(val);})
std::string valueAsString(const T& value) {
	return std::to_string(value);
}
template<>
inline std::string valueAsString(const std::string& value) {
	return value;
}
template<class OptionType>
struct OptionArgs {
	std::optional<std::remove_pointer_t<OptionType>> default_value;
	const char* description = "";
	bool include_in_frontend = true;
	bool is_option = true;
};
class ProgramOptionBase {
public:
	virtual ~ProgramOptionBase() = default;
	constexpr ProgramOptionBase(std::string token) :token(token) {}
	const std::string token;
	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) = 0;
	virtual std::string string() const = 0;
	virtual bool isOption() const = 0;
	virtual bool includeInFrontend() const { return true; };
};
template<typename T>
concept IStreamAble = requires (std::istream& istream, T& t) { istream >> t; };
template<typename T>
concept ValidProgramOption = !std::is_pointer_v<T> && IStreamAble<T>;
template<typename T>
concept ValidProgramOptionPtr = std::is_pointer_v<T> && IStreamAble<std::remove_pointer_t<T>>;

template<typename OptionType>
requires (ValidProgramOption<OptionType>)
class ProgramOption : public ProgramOptionBase {
public:
	ProgramOption(std::string token, OptionType default_value, OptionArgs<OptionType> args = {}) :
		ProgramOptionBase(token), default_value(default_value), description(args.description), value(std::make_shared<OptionType>(default_value)), is_option(args.is_option) {
	}
	// ProgramOption(std::string token, std::shared_ptr<OptionType>&& value_ptr, OptionType default_value, std::string description = "")
	// 		: ProgramOptionBase(token), default_value(default_value), description(description), value(value_ptr) {
	// }
	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) override {
		if (!is_option)
			return;
		options.add(boost::make_shared<boost::program_options::option_description>( boost::program_options::option_description(this->token.c_str(), boost::program_options::value<OptionType>(value.get())->default_value(default_value), this->description ? description.value().c_str() : "")));
	}
	virtual std::string string() const override {
		return valueAsString(*value);
	}
	virtual bool isOption() const override { return this->is_option; };
private:
	bool is_option;
	std::shared_ptr<OptionType> value;
	OptionType default_value;
	const std::optional<const std::string> description;
};

template<typename OptionPtr>
requires (ValidProgramOptionPtr<OptionPtr>)
class ProgramOptionPtr : public ProgramOptionBase {
using OptionType = std::remove_pointer_t<OptionPtr>;
public:
	ProgramOptionPtr(std::string token, OptionPtr value_ptr, OptionArgs<OptionPtr> args = {})
			: ProgramOptionBase(token), value_ptr(value_ptr), default_value(args.default_value), description(args.description), include_in_frontend(args.include_in_frontend), is_option(args.is_option) {
	}
	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) override {
		if (!is_option)
			return;
		if (this->default_value)
			options.add(boost::make_shared<boost::program_options::option_description>( boost::program_options::option_description(this->token.c_str(), boost::program_options::value(value_ptr)->default_value(this->default_value.value()), this->description)));
		else
			options.add(boost::make_shared<boost::program_options::option_description>( boost::program_options::option_description(this->token.c_str(), boost::program_options::value(value_ptr), this->description)));
	}
	virtual std::string string() const override {
		return valueAsString(*value_ptr);
	}
	virtual bool isOption() const override { return this->is_option; };
	virtual bool includeInFrontend() const override { return this->include_in_frontend; };
private:
	bool is_option;
	OptionPtr value_ptr;
	std::optional<OptionType> default_value;
	const char* description;
	bool include_in_frontend;
};

// template<typename OptionType>
// requires (IStreamAble<OptionType>)
// class ProgramConstant : public ProgramOptionBase {
// public:
// 	ProgramConstant(std::string token, OptionType default_value, std::string description = "") :
// 		ProgramOptionBase(token), default_value(default_value), description(description), value(default_value) {
// 	}
// 	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) override {}
// 	virtual std::string string() const override {
// 		return valueAsString(value);
// 	}
// 	virtual bool isOption() const override { return false; };
// private:
// 	OptionType value;
// 	OptionType default_value;
// 	const std::optional<const std::string> description;
// };

class ProgramOptions {
	class EasyOptionAdder {
	public:
		EasyOptionAdder(ProgramOptions* server_options) : server_options(server_options) {}

		template<class OptionTypeOrPtr>
		EasyOptionAdder& operator()(std::string token, OptionTypeOrPtr value, OptionArgs<OptionTypeOrPtr> args = {}) {
			server_options->add(token, value, args);
			return *this;
		}
	private:
		ProgramOptions* server_options;
	};
public:
	template<class OptionType>
	requires (ValidProgramOption<OptionType>)
	void add(std::string token, OptionType value, OptionArgs<OptionType> args = {}) {
		this->addOption(std::make_unique<ProgramOption<OptionType>>(token, value, args));
	}
	template<class OptionPtr>
	requires (ValidProgramOptionPtr<OptionPtr>)
	void add(std::string token, OptionPtr value_ptr, OptionArgs<OptionPtr> args = {}) {
		this->addOption(std::make_unique<ProgramOptionPtr<OptionPtr>>(token, value_ptr, args));
	}

	void addOption(std::unique_ptr<ProgramOptionBase> program_option) {
		this->program_options.push_back(std::move(program_option));
	}
	EasyOptionAdder addOptions() { return EasyOptionAdder(this); }
	std::vector<std::unique_ptr<ProgramOptionBase>>& get() { return program_options; }
	const std::vector<std::unique_ptr<ProgramOptionBase>>& get() const { return program_options; }
private:
	std::vector<std::unique_ptr<ProgramOptionBase>> program_options;
};
} // export namespace FuzeHttp
