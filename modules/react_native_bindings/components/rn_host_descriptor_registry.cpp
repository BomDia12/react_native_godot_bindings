#include "rn_host_descriptor_registry.h"

bool RNHostDescriptorRegistry::register_descriptor(const std::shared_ptr<const RNHostDescriptor> &p_descriptor, RNError &r_error) {
	if (frozen) {
		r_error = RNError::make(RNErrorCode::DUPLICATE_REGISTRATION, "host descriptor definitions are frozen", "registerDescriptor");
		return false;
	}
	if (!p_descriptor || p_descriptor->get_name().is_empty()) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "host descriptor name is empty", "registerDescriptor");
		return false;
	}
	const StringName name = p_descriptor->get_name();
	if (name == "__proto__" || name == "constructor" || descriptors.has(name)) {
		r_error = RNError::make(RNErrorCode::DUPLICATE_REGISTRATION, vformat("host descriptor '%s' is reserved or already registered", name), "registerDescriptor");
		return false;
	}
	descriptors[name] = p_descriptor;
	return true;
}

std::shared_ptr<const RNHostDescriptor> RNHostDescriptorRegistry::find(const StringName &p_name) const {
	const std::shared_ptr<const RNHostDescriptor> *descriptor = descriptors.getptr(p_name);
	return descriptor ? *descriptor : nullptr;
}

Dictionary RNHostDescriptorRegistry::get_view_config(const StringName &p_name) const {
	const std::shared_ptr<const RNHostDescriptor> descriptor = find(p_name);
	return descriptor ? descriptor->get_view_config().duplicate(true) : Dictionary();
}

Dictionary RNHostDescriptorRegistry::get_constants() const {
	Dictionary configs;
	Array names;
	for (const KeyValue<StringName, std::shared_ptr<const RNHostDescriptor>> &entry : descriptors) {
		configs[entry.key] = entry.value->get_view_config().duplicate(true);
		names.push_back(String(entry.key));
	}
	Dictionary result;
	result["ViewManagerNames"] = names;
	result["ViewManagerConfigs"] = configs;
	return result;
}
