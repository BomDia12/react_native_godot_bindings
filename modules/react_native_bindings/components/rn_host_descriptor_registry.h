#pragma once

#include "rn_host_descriptor.h"

#include "core/templates/hash_map.h"

class RNHostDescriptorRegistry {
	HashMap<StringName, std::shared_ptr<const RNHostDescriptor>> descriptors;
	bool frozen = false;

public:
	bool register_descriptor(const std::shared_ptr<const RNHostDescriptor> &p_descriptor, RNError &r_error);
	std::shared_ptr<const RNHostDescriptor> find(const StringName &p_name) const;
	bool has(const StringName &p_name) const { return descriptors.has(p_name); }
	Dictionary get_view_config(const StringName &p_name) const;
	Dictionary get_constants() const;
	void freeze() { frozen = true; }
	bool is_frozen() const { return frozen; }
	int size() const { return descriptors.size(); }
};
