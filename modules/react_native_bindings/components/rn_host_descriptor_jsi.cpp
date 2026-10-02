#include "rn_host_descriptor_jsi.h"

#include "../interop/rn_value_codec.h"
#include "rn_host_descriptor_registry.h"

namespace {

std::string utf8(const String &p_value) {
	const CharString encoded = p_value.utf8();
	return std::string(encoded.get_data(), encoded.length());
}

facebook::jsi::Value dictionary_to_js(facebook::jsi::Runtime &p_runtime, const Dictionary &p_dictionary) {
	facebook::jsi::Value result;
	RNError error;
	if (!RNValueCodec::to_js(p_runtime, p_dictionary, RNValueSchema::value(RNValueType::DYNAMIC), result, error, nullptr, "HostDescriptorRegistry.metadata")) {
		throw facebook::jsi::JSError(p_runtime, utf8(error.describe()));
	}
	return result;
}

String js_string(facebook::jsi::Runtime &p_runtime, const facebook::jsi::Value *p_arguments, size_t p_count, const char *p_operation) {
	if (p_count != 1 || !p_arguments[0].isString()) {
		throw facebook::jsi::JSError(p_runtime, std::string(p_operation) + " expects one component-name string.");
	}
	const std::string value = p_arguments[0].getString(p_runtime).utf8(p_runtime);
	return String::utf8(value.data(), int(value.size()));
}

} // namespace

facebook::jsi::Value RNHostDescriptorJSIRegistry::get(facebook::jsi::Runtime &p_runtime, const facebook::jsi::PropNameID &p_name) {
	const std::string name = p_name.utf8(p_runtime);
	if (name == "hasComponent") {
		return facebook::jsi::Function::createFromHostFunction(
				p_runtime, facebook::jsi::PropNameID::forAscii(p_runtime, "hasComponent"), 1,
				[this](facebook::jsi::Runtime &rt, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t count) {
					return facebook::jsi::Value(registry && registry->has(js_string(rt, args, count, "hasComponent")));
				});
	}
	if (name == "getConfig") {
		return facebook::jsi::Function::createFromHostFunction(
				p_runtime, facebook::jsi::PropNameID::forAscii(p_runtime, "getConfig"), 1,
				[this](facebook::jsi::Runtime &rt, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t count) {
					return dictionary_to_js(rt, registry ? registry->get_view_config(js_string(rt, args, count, "getConfig")) : Dictionary());
				});
	}
	if (name == "getConstants") {
		return facebook::jsi::Function::createFromHostFunction(
				p_runtime, facebook::jsi::PropNameID::forAscii(p_runtime, "getConstants"), 0,
				[this](facebook::jsi::Runtime &rt, const facebook::jsi::Value &, const facebook::jsi::Value *, size_t) {
					return dictionary_to_js(rt, registry ? registry->get_constants() : Dictionary());
				});
	}
	return facebook::jsi::Value::undefined();
}

std::vector<facebook::jsi::PropNameID> RNHostDescriptorJSIRegistry::getPropertyNames(facebook::jsi::Runtime &p_runtime) {
	std::vector<facebook::jsi::PropNameID> names;
	for (const char *name : { "hasComponent", "getConfig", "getConstants" }) {
		names.push_back(facebook::jsi::PropNameID::forAscii(p_runtime, name));
	}
	return names;
}
