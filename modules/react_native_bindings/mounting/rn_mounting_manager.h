#pragma once

#include "../fabric/rn_registry.h"
#include "../runtime/react_native_runtime_coordinator.h"
#include "rn_layout_tree.h"
#include "rn_mounting_transaction.h"
#include "rn_tree_differ.h"

#include "core/templates/hash_set.h"

#include <memory>

class Control;
class ReactNativeRootView;

struct RNMountedNode {
	RNMountedNodeSnapshot snapshot;
	int logical_parent_tag = 0;
	int native_parent_tag = 0;
	int native_index = -1;
	ObjectID object_id;
	String view_name;
	Ref<RNShadowNode> shadow_node;
	Dictionary declarative_props;
	Dictionary direct_prop_overrides;
	uint64_t last_changed_revision = 0;
	bool renderer_owned = false;
};

struct RNMountingManagerStats {
	RNTreeDifferStats differ;
	uint64_t hosts_created = 0;
	uint64_t hosts_freed = 0;
	uint64_t layout_boxes_applied = 0;
	uint64_t rejected_transactions = 0;
	uint64_t rollbacks = 0;
	uint64_t mutations[5] = {};
};

class RNMountingManager {
	ReactNativeRootView *owner = nullptr;
	RNRegistry registry;
	RNLayoutTree layout_tree;
	HashMap<int, RNMountedNode> mounted_nodes;
	HashMap<int, Dictionary> direct_prop_overrides;
	Ref<RNShadowNode> declarative_root;
	Ref<RNShadowNode> committed_root;
	std::shared_ptr<const RNSurfaceSnapshot> published_snapshot;
	ObjectID mount_container_id;
	int root_tag = 0;
	uint64_t runtime_generation = 0;
	uint64_t surface_epoch = 0;
	uint64_t published_revision = 0;
	bool transaction_in_flight = false;
	HashMap<int, ObjectID> prepared_hosts;
	int fail_before_mutation = -1;
	int fail_after_mutation = -1;
	RNMountingManagerStats stats;

	Control *mount_container() const;
	Control *native_parent(int p_tag) const;
	Control *host_for_tag(int p_tag) const;
	void ensure_mount_container();
	Ref<RNShadowNode> build_effective_tree(const Ref<RNShadowNode> &p_node, const HashMap<int, Dictionary> &p_overrides) const;
	void reconcile_overrides(const Ref<RNShadowNode> &p_node, HashMap<int, Dictionary> &r_overrides, HashSet<int> &r_live_tags) const;
	Control *create_host(const Ref<RNShadowNode> &p_node);
	void apply_host_props(Control *p_host, const Ref<RNShadowNode> &p_node, bool p_branch_targetable);
	void apply_layout(Control *p_host, const Rect2 &p_layout);
	bool prepare_transaction(RNMountingTransaction &r_transaction, const Ref<RNShadowNode> &p_next_root, const Size2 &p_constraint, String &r_error);
	bool apply_transaction(RNMountingTransaction &p_transaction, const Ref<RNShadowNode> &p_declarative_root, const HashMap<int, Dictionary> &p_next_overrides, const Size2 &p_constraint, const Transform2D &p_window_transform, Vector<RNNativeEvent> &r_events, String &r_error);
	void destroy_detached_hosts(RNMountingTransaction &p_transaction);
	void restore_scene(const HashMap<int, RNMountedNode> &p_records, const Ref<RNShadowNode> &p_root, const Size2 &p_constraint);
	std::shared_ptr<RNSurfaceSnapshot> build_snapshot(const Ref<RNShadowNode> &p_root, const Ref<RNShadowNode> &p_declarative_root, const HashMap<int, Dictionary> &p_overrides, const HashMap<int, Rect2> &p_layouts, const Transform2D &p_window_transform, uint64_t p_revision, HashMap<int, RNMountedNode> &r_records) const;
	void build_snapshot_node(const Ref<RNShadowNode> &p_node, int p_logical_parent, int p_native_parent, const Point2 &p_parent_origin, bool p_branch_targetable, const HashMap<int, Dictionary> &p_overrides, const HashMap<int, Rect2> &p_layouts, const Transform2D &p_window_transform, uint64_t p_revision, RNSurfaceSnapshot &r_snapshot, HashMap<int, RNMountedNode> &r_records, HashMap<int, int> &r_native_indices) const;
	bool hierarchy_matches(const HashMap<int, RNMountedNode> &p_records) const;
	void queue_layout_events(const std::shared_ptr<const RNSurfaceSnapshot> &p_old_snapshot, const RNSurfaceSnapshot &p_new_snapshot, Vector<RNNativeEvent> &r_events) const;

public:
	explicit RNMountingManager(ReactNativeRootView *p_owner);
	~RNMountingManager();

	void attach(uint64_t p_generation, int p_root_tag, uint64_t p_surface_epoch);
	void clear(bool p_keep_container = false);
	bool commit(const RNPendingCommit &p_commit, const Size2 &p_constraint, const Transform2D &p_window_transform, Vector<RNNativeEvent> &r_events, String &r_error);
	bool resize(const Size2 &p_constraint, const Transform2D &p_window_transform, Vector<RNNativeEvent> &r_events, String &r_error);
	bool apply_direct_props(int p_tag, const Dictionary &p_patch, const Size2 &p_constraint, const Transform2D &p_window_transform, Vector<RNNativeEvent> &r_events, String &r_error);
	void publish_transform(const Transform2D &p_window_transform);

	const Ref<RNShadowNode> &get_committed_root() const { return committed_root; }
	const RNRegistry &get_registry() const { return registry; }
	RNRegistry &get_registry() { return registry; }
	std::shared_ptr<const RNSurfaceSnapshot> get_snapshot() const { return published_snapshot; }
	uint64_t get_published_revision() const { return published_revision; }
	const RNMountingManagerStats &get_stats() const { return stats; }
	void reset_stats() {
		stats = RNMountingManagerStats();
		layout_tree.reset_stats();
	}
	void set_failure_injection(int p_before_mutation, int p_after_mutation) {
		fail_before_mutation = p_before_mutation;
		fail_after_mutation = p_after_mutation;
	}
};
