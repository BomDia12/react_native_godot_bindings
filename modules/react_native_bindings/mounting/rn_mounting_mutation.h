#pragma once

#include "../fabric/rn_shadow_node.h"

#include "core/templates/vector.h"
#include "core/variant/dictionary.h"

#include <cstdint>

enum class RNMutationType : uint8_t {
	CREATE,
	DELETE,
	INSERT,
	REMOVE,
	UPDATE,
};

struct RNMountContext {
	int native_parent_tag = 0;
	bool branch_targetable = true;
	int text_host_tag = 0;

	bool operator==(const RNMountContext &p_other) const {
		return native_parent_tag == p_other.native_parent_tag && branch_targetable == p_other.branch_targetable && text_host_tag == p_other.text_host_tag;
	}

	bool operator!=(const RNMountContext &p_other) const {
		return !(*this == p_other);
	}
};

struct RNPropPatch {
	Dictionary changed;
	Vector<String> removed;
	bool inherited_context_changed = false;

	bool is_empty() const {
		return changed.is_empty() && removed.is_empty() && !inherited_context_changed;
	}
};

struct RNMountingMutation {
	RNMutationType type = RNMutationType::CREATE;
	int parent_tag = 0;
	int index = -1;
	Ref<RNShadowNode> old_node;
	Ref<RNShadowNode> new_node;
};

const char *rn_mutation_type_name(RNMutationType p_type);
RNPropPatch rn_diff_props(const Dictionary &p_old_props, const Dictionary &p_new_props, bool p_context_changed = false);
