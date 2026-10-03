#include "rn_builtin_native_modules.h"

#include "rn_image_service.h"

#include "core/config/engine.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "servers/display/display_server.h"

namespace {

RNRecordFieldSchema field(const StringName &p_name, RNValueType p_type, bool p_nullable = false) {
	RNRecordFieldSchema result;
	result.name = p_name;
	result.value = std::make_shared<RNValueSchema>(RNValueSchema::value(p_type));
	result.nullable = p_nullable;
	return result;
}

RNValueSchema version_schema() {
	Vector<RNRecordFieldSchema> fields;
	fields.push_back(field("major", RNValueType::INTEGER));
	fields.push_back(field("minor", RNValueType::INTEGER));
	fields.push_back(field("patch", RNValueType::INTEGER));
	fields.push_back(field("prerelease", RNValueType::STRING, true));
	return RNValueSchema::record(fields);
}

RNValueSchema constants_schema() {
	Vector<RNRecordFieldSchema> fields;
	fields.push_back(field("isTesting", RNValueType::BOOL));
	RNRecordFieldSchema rn_version;
	rn_version.name = "reactNativeVersion";
	rn_version.value = std::make_shared<RNValueSchema>(version_schema());
	fields.push_back(rn_version);
	fields.push_back(field("Version", RNValueType::STRING));
	fields.push_back(field("godotVersion", RNValueType::STRING));
	fields.push_back(field("hostOS", RNValueType::STRING));
	fields.push_back(field("assetScale", RNValueType::FLOAT));
	return RNValueSchema::record(fields);
}

class RNPlatformConstantsModule : public RNNativeModule {
public:
	RNModuleResult invoke_sync(const StringName &p_method, const Array &, const RNCallContext &) override {
		if (p_method != "getConstants") {
			return RNModuleResult::failure(RNError::make(RNErrorCode::UNSUPPORTED, "unknown PlatformConstants method", "PlatformConstants." + String(p_method)));
		}
		const Dictionary version = Engine::get_singleton()->get_version_info();
		const String godot_version = vformat("%d.%d.%d", version.get("major", 0), version.get("minor", 0), version.get("patch", 0));
		double asset_scale = 1.0;
		if (DisplayServer::get_singleton() && DisplayServer::get_singleton()->get_name() != "headless") {
			const double reported = DisplayServer::get_singleton()->screen_get_scale();
			if (reported > 0 && Math::is_finite(reported)) {
				asset_scale = reported;
			}
		}
		Dictionary rn_version;
		rn_version["major"] = 0;
		rn_version["minor"] = 87;
		rn_version["patch"] = 1;
		rn_version["prerelease"] = Variant();
		Dictionary constants;
#ifdef DEBUG_ENABLED
		constants["isTesting"] = true;
#else
		constants["isTesting"] = false;
#endif
		constants["reactNativeVersion"] = rn_version;
		constants["Version"] = godot_version;
		constants["godotVersion"] = godot_version;
		constants["hostOS"] = OS::get_singleton()->get_name();
		constants["assetScale"] = asset_scale;
		return RNModuleResult::success(constants);
	}
};

} // namespace

bool rn_register_builtin_native_modules(RNNativeModuleRegistry &p_registry, RNError &r_error) {
	RNMethodSchema get_constants;
	get_constants.name = "getConstants";
	get_constants.result = constants_schema();
	RNModuleDefinition definition;
	definition.name = "PlatformConstants";
	definition.methods.push_back(get_constants);
	definition.factory = []() { return std::make_unique<RNPlatformConstantsModule>(); };
	return p_registry.register_module(definition, r_error) && rn_register_image_module(p_registry, r_error);
}
