module;
#include <boost/container/container_fwd.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <sodium.h>
#include <boost/hash2/md5.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include <boost/smart_ptr.hpp>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
export module FuzeHttp.Utils;

export namespace FuzeHttp{
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

class TemplateMacro {
public:
	constexpr TemplateMacro(std::string token) :token(token) {}
	const std::string token;
	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) = 0;
	virtual std::string string() const = 0;
	virtual bool isOption() const = 0;
	virtual bool includeInFrontend() const { return true; };
};

template<typename OptionType>
requires (std::is_convertible_v<std::remove_pointer_t<OptionType>, std::string> || requires(std::remove_pointer_t<OptionType> o){std::to_string(o);})
class TemplateOption : public TemplateMacro {
public:
	TemplateOption(std::string token, OptionType default_value, std::string description = "") :
		TemplateMacro(token), default_value(default_value), description(description), value(std::make_shared<OptionType>(default_value)) {
	}
	// TemplateOption(std::string token, std::shared_ptr<OptionType>&& value_ptr, OptionType default_value, std::string description = "")
	// 		: TemplateMacro(token), default_value(default_value), description(description), value(value_ptr) {
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
requires (std::is_convertible_v<std::remove_pointer_t<OptionType>, std::string> || requires(std::remove_pointer_t<OptionType> o){std::to_string(o);})
class TemplateOptionPtr : public TemplateMacro {
public:
	struct Args {
		std::optional<OptionType> default_value;
		std::optional<const char*> description;
		bool include_in_frontend = true;
	};
	// TemplateOptionPtr(std::string token, OptionType* value_ptr, OptionType default_value, std::string description = "")
	// 		: TemplateMacro(token), default_value(default_value), description(description), value_ptr(value_ptr) {
	// }
	TemplateOptionPtr(std::string token, OptionType* value_ptr, Args args = {})
			: TemplateMacro(token), value_ptr(value_ptr), /*typed_value(value_ptr),*/ default_value(args.default_value), description(args.description), include_in_frontend(args.include_in_frontend) {
		// if (this->default_value)
		// 	this->typed_value.default_value(this->default_value.value());
		// this->value_semantic = std::make_shared<boost::program_options::value_semantic*>(&(this->typed_value));
		// this->value_semantic = &(this->typed_value);
	}
	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) override {
		// boost::program_options::typed_value value(value_ptr);
		// boost::program_options::typed_value value = boost::program_options::value<OptionType>(value_ptr);
		// boost::program_options::value_semantic* value_semantic = &value;
		// options.add(boost::make_shared<boost::program_options::option_description>( boost::program_options::option_description(this->token.c_str(), *value_semantic.get(), this->description ? description.value() : "")));
		if (this->default_value)
			options.add(boost::make_shared<boost::program_options::option_description>( boost::program_options::option_description(this->token.c_str(), boost::program_options::value(value_ptr)->default_value(this->default_value.value()), this->description ? description.value() : "")));
		else
			options.add(boost::make_shared<boost::program_options::option_description>( boost::program_options::option_description(this->token.c_str(), boost::program_options::value(value_ptr), this->description ? description.value() : "")));
	}
	virtual std::string string() const override {
		return valueAsString(*value_ptr);
	}
	virtual bool isOption() const override { return true; };
	virtual bool includeInFrontend() const override { return this->include_in_frontend; };
private:
	OptionType* value_ptr;
	// boost::program_options::typed_value<OptionType> typed_value;
	// boost::program_options::value_semantic* value_semantic;
	// std::shared_ptr<boost::program_options::value_semantic*> value_semantic;
	std::optional<OptionType> default_value;
	const std::optional<const char*> description;
	bool include_in_frontend;
};

template<typename OptionType>
requires (std::is_convertible_v<OptionType, std::string> || requires(OptionType o){std::to_string(o);})
class TemplateConstant : public TemplateMacro {
public:
	TemplateConstant(std::string token, OptionType default_value, std::string description = "") :
		TemplateMacro(token), default_value(default_value), description(description), value(default_value) {
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

inline bool fileNameEndsWith(const std::string& file_name, const std::string& delimiter) {
	int file_extension_index;
	if ((file_extension_index = file_name.rfind(".")) == -1) {
		file_extension_index = file_name.size();
	}
	return file_name.substr(0, file_extension_index).ends_with(delimiter);
}

inline std::string insertExtensionToFileName(const std::string& file_name, const std::string& extension) {
	int file_extension_index;
	if ((file_extension_index = file_name.rfind(".")) == -1) {
		file_extension_index = file_name.size();
	}
	std::string new_file_name = file_name.substr(0, file_extension_index) + extension + file_name.substr(file_extension_index);
	return new_file_name;
}

inline bool isValidURLParameter(const std::string& parameter) {
	if (parameter.length() == 0) return false;
	// https://stackoverflow.com/a/2926983/18658154
	return find_if(parameter.begin(), parameter.end(),
		[](char c) { return !(isalnum(c) || (c == '_') || (c == '-')); }) == parameter.end();
}

template<typename BoostHashType, typename StringType>
requires (requires(BoostHashType hasher, const StringType& str){hasher.update(str.c_str(), str.length());})
std::string getHash(const StringType& source_data) {
	BoostHashType hash;
	hash.update(source_data.c_str(), source_data.length());
	char hash_base64[sodium_base64_ENCODED_LEN(128 / 8, sodium_base64_VARIANT_URLSAFE_NO_PADDING)];
	sodium_bin2base64(
		hash_base64, sizeof hash_base64,
		hash.result().data(), hash.result().size(),
		// (unsigned char*)key_bytes, 20,
		sodium_base64_VARIANT_URLSAFE_NO_PADDING
	);
	return hash_base64;
}

inline std::string getEtagFromFile(const std::filesystem::path& file) {
	std::string file_last_modified = std::to_string(std::filesystem::last_write_time(file).time_since_epoch().count());
	return getHash<boost::hash2::md5_128>(file_last_modified);
}

std::string writeManifestJson(const std::filesystem::path& manifest_file, const std::unordered_map<std::string /*target*/, std::string /*etag*/>& manifest_frontend_etags, const std::string& combined_hash) {
	boost::json::object manifest_obj;
	boost::json::object manifest_frontend_obj;
	for (const auto& target : manifest_frontend_etags)
		manifest_frontend_obj.emplace(target.first, target.second);
	manifest_obj.emplace("frontend", manifest_frontend_obj);
	manifest_obj.emplace("combined_hash", combined_hash);
	std::ofstream manifest_json_out(manifest_file);
	std::string json_as_str = boost::json::serialize(manifest_obj);
	manifest_json_out.write(json_as_str.c_str(), json_as_str.length());
	return json_as_str;
}
}; // namespace FuzeHttp
