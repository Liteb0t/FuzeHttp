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

class ProgramOptionBase {
public:
	constexpr ProgramOptionBase(std::string token) :token(token) {}
	const std::string token;
	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) = 0;
	virtual std::string string() const = 0;
	virtual bool isOption() const = 0;
	virtual bool includeInFrontend() const { return true; };
};
template<typename T>
concept IStreamAble = requires (std::istream& istream, T& t) { istream >> t; };

template<typename OptionType>
requires (IStreamAble<OptionType>)
class ProgramOption : public ProgramOptionBase {
public:
	ProgramOption(std::string token, OptionType default_value, std::string description = "") :
		ProgramOptionBase(token), default_value(default_value), description(description), value(std::make_shared<OptionType>(default_value)) {
	}
	// ProgramOption(std::string token, std::shared_ptr<OptionType>&& value_ptr, OptionType default_value, std::string description = "")
	// 		: ProgramOptionBase(token), default_value(default_value), description(description), value(value_ptr) {
	// }
	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) override {
		options.add(boost::make_shared<boost::program_options::option_description>( boost::program_options::option_description(this->token.c_str(), boost::program_options::value<OptionType>(value.get())->default_value(default_value), this->description ? description.value().c_str() : "")));
	}
	virtual std::string string() const override {
		return valueAsString(*value);
	}
	virtual bool isOption() const override { return true; };
private:
	std::shared_ptr<OptionType> value;
	OptionType default_value;
	const std::optional<const std::string> description;
};

template<typename OptionType>
requires (IStreamAble<std::remove_pointer_t<OptionType>>)
class ProgramOptionPtr : public ProgramOptionBase {
public:
	struct Args {
		std::optional<OptionType> default_value;
		const char* description = "";
		bool include_in_frontend = true;
	};
	ProgramOptionPtr(std::string token, OptionType* value_ptr, Args args = {})
			: ProgramOptionBase(token), value_ptr(value_ptr), default_value(args.default_value), description(args.description), include_in_frontend(args.include_in_frontend) {
	}
	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) override {
		if (this->default_value)
			options.add(boost::make_shared<boost::program_options::option_description>( boost::program_options::option_description(this->token.c_str(), boost::program_options::value(value_ptr)->default_value(this->default_value.value()), this->description)));
		else
			options.add(boost::make_shared<boost::program_options::option_description>( boost::program_options::option_description(this->token.c_str(), boost::program_options::value(value_ptr), this->description)));
	}
	virtual std::string string() const override {
		return valueAsString(*value_ptr);
	}
	virtual bool isOption() const override { return true; };
	virtual bool includeInFrontend() const override { return this->include_in_frontend; };
private:
	OptionType* value_ptr;
	std::optional<OptionType> default_value;
	const char* description;
	bool include_in_frontend;
};

template<typename OptionType>
requires (IStreamAble<OptionType>)
class ProgramConstant : public ProgramOptionBase {
public:
	ProgramConstant(std::string token, OptionType default_value, std::string description = "") :
		ProgramOptionBase(token), default_value(default_value), description(description), value(default_value) {
	}
	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) override {}
	virtual std::string string() const override {
		return valueAsString(value);
	}
	virtual bool isOption() const override { return false; };
private:
	OptionType value;
	OptionType default_value;
	const std::optional<const std::string> description;
};

class ProgramOptions {
	class EasyOptionAdder {
	public:
		EasyOptionAdder(ProgramOptions* server_options) : server_options(server_options) {}

		EasyOptionAdder& operator()(ProgramOptionBase* program_option) {
			server_options->addOption(program_option);
			return *this;
		}
	private:
		ProgramOptions* server_options;
	};
public:
	void addOption(ProgramOptionBase* program_option) { this->program_options.push_back(program_option); }
	EasyOptionAdder addOptions() { return EasyOptionAdder(this); }
	std::vector<ProgramOptionBase*> get() { return program_options; }
private:
	std::vector<ProgramOptionBase*> program_options;
};
} // export namespace FuzeHttp
