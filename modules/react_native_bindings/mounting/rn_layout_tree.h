#pragma once

#include "../components/rn_host_descriptor.h"
#include "../fabric/rn_shadow_node.h"

#include "core/math/rect2.h"
#include "core/templates/hash_map.h"

#include <memory>
#include <unordered_map>
#include <vector>

struct YGNode;
using YGNodeRef = YGNode *;
struct YGConfig;
using YGConfigRef = YGConfig *;

struct RNLayoutTreeStats {
	uint64_t nodes_created = 0;
	uint64_t nodes_freed = 0;
	uint64_t style_writes = 0;
	uint64_t text_nodes_dirtied = 0;
	uint64_t calculations = 0;
};

struct RNLayoutMeasureContext {
	int tag = 0;
	String text;
	Dictionary props;
	RNPreparedHostState prepared_state;
	std::shared_ptr<const RNHostDescriptor> descriptor;
	bool measure_initialized = false;
};

class RNLayoutTree {
	struct Record {
		YGNodeRef yoga_node = nullptr;
		int parent_tag = 0;
		Dictionary props;
		std::unique_ptr<RNLayoutMeasureContext> context;
	};

	YGConfigRef config = nullptr;
	std::unordered_map<int, Record> records;
	std::vector<Record> prepared_removed_records;
	Ref<RNShadowNode> published_root;
	Ref<RNShadowNode> prepared_root;
	Size2 published_constraint;
	Size2 prepared_constraint;
	HashMap<int, Rect2> layouts;
	RNLayoutTreeStats stats;

	YGNodeRef prepare_node(const Ref<RNShadowNode> &p_node, int p_parent_tag, const HashMap<int, RNPreparedHostState> *p_prepared_states, HashMap<int, bool> &r_seen, String &r_error);
	void capture_layout(YGNodeRef p_node, HashMap<int, Rect2> &r_layouts) const;
	void free_prepared_removed();
	void free_all();

public:
	RNLayoutTree();
	~RNLayoutTree();

	bool prepare(const Ref<RNShadowNode> &p_root, const Size2 &p_constraint, HashMap<int, Rect2> &r_layouts, String &r_error, const HashMap<int, RNPreparedHostState> *p_prepared_states = nullptr);
	void publish();
	bool rebuild(const Ref<RNShadowNode> &p_root, const Size2 &p_constraint, String &r_error);
	void clear();

	const RNLayoutTreeStats &get_stats() const { return stats; }
	void reset_stats() { stats = RNLayoutTreeStats(); }
};
