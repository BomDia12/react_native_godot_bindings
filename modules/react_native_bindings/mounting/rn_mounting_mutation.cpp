#include "rn_mounting_mutation.h"

const char *rn_mutation_type_name(RNMutationType p_type) {
	switch (p_type) {
		case RNMutationType::CREATE:
			return "create";
		case RNMutationType::DELETE:
			return "delete";
		case RNMutationType::INSERT:
			return "insert";
		case RNMutationType::REMOVE:
			return "remove";
		case RNMutationType::UPDATE:
			return "update";
	}
	return "unknown";
}

RNPropPatch rn_diff_props(const Dictionary &p_old_props, const Dictionary &p_new_props, bool p_context_changed) {
	RNPropPatch patch;
	patch.inherited_context_changed = p_context_changed;
	const Array new_keys = p_new_props.keys();
	for (int i = 0; i < new_keys.size(); ++i) {
		const Variant key = new_keys[i];
		if (!p_old_props.has(key) || p_old_props[key] != p_new_props[key]) {
			patch.changed[key] = p_new_props[key];
		}
	}
	const Array old_keys = p_old_props.keys();
	for (int i = 0; i < old_keys.size(); ++i) {
		const Variant key = old_keys[i];
		if (!p_new_props.has(key)) {
			patch.removed.push_back(String(key));
		}
	}
	return patch;
}
