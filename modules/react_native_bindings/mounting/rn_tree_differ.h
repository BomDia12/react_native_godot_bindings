#pragma once

#include "rn_mounting_mutation.h"

struct RNTreeDifferStats {
	uint64_t visited_nodes = 0;
	uint64_t child_maps_allocated = 0;
};

class RNTreeDiffer {
public:
	static bool diff(const Ref<RNShadowNode> &p_old_root, const Ref<RNShadowNode> &p_new_root, Vector<RNMountingMutation> &r_mutations, String &r_error, RNTreeDifferStats *r_stats = nullptr);
};
