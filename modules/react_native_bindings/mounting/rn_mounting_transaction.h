#pragma once

#include "../components/rn_host_descriptor.h"
#include "rn_mounting_mutation.h"

#include "core/math/rect2.h"
#include "core/object/object_id.h"
#include "core/templates/hash_map.h"

struct RNMountingTransaction {
	uint64_t runtime_generation = 0;
	int root_tag = 0;
	uint64_t surface_epoch = 0;
	uint64_t revision = 0;
	Ref<RNShadowNode> old_root;
	Ref<RNShadowNode> new_root;
	Vector<RNMountingMutation> mutations;
	HashMap<int, Rect2> prepared_layouts;
	HashMap<int, RNPreparedHostState> prepared_states;
	HashMap<int, Variant> captured_native_states;
	Vector<ObjectID> detached_new_hosts;
};
