#pragma once

#include "../fabric/rn_native_event.h"
#include "../fabric/rn_shadow_node.h"

#include "core/math/rect2.h"
#include "core/math/vector4.h"
#include "core/object/object.h"
#include "core/object/object_id.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <unordered_map>

class FabricUIManager;
class NativeDOM;
class ReactNativeRootView;

enum class RNSurfaceStatus {
	REGISTERED,
	STARTING,
	ACTIVE,
	STOPPING,
	FAILED,
	DETACHED,
};

enum class RNBundleStatus {
	UNEVALUATED,
	EVALUATING,
	READY,
	FAILED,
};

struct RNSurfaceTag {
	int root_tag = 0;
	int tag = 0;

	bool operator==(const RNSurfaceTag &p_other) const {
		return root_tag == p_other.root_tag && tag == p_other.tag;
	}
};

struct RNSurfaceTagHash {
	size_t operator()(const RNSurfaceTag &p_value) const {
		return (static_cast<size_t>(static_cast<uint32_t>(p_value.root_tag)) << 32) ^ static_cast<uint32_t>(p_value.tag);
	}
};

struct RNSurfaceRoute {
	int root_tag = 0;
	ObjectID root_view_id;
	String application_key;
	uint64_t runtime_generation = 0;
	uint64_t surface_epoch = 0;
	uint64_t desired_revision = 0;
	uint64_t mounted_revision = 0;
	RNSurfaceStatus status = RNSurfaceStatus::REGISTERED;
	String error;
};

struct RNMountedNodeSnapshot {
	int tag = 0;
	int parent_tag = 0;
	Vector<int> child_tags;
	String view_name;
	String native_id;
	String text_content;
	Rect2 local_rect;
	Rect2 root_rect;
	Rect2 window_rect;
	Vector4 border_width;
	Size2 inner_size;
	int offset_parent_tag = 0;
	Point2 offset;
	ObjectID object_id;
	Ref<RNShadowNode> shadow_node;
};

struct RNSurfaceSnapshot {
	int root_tag = 0;
	uint64_t runtime_generation = 0;
	uint64_t surface_epoch = 0;
	uint64_t revision = 0;
	HashMap<int, RNMountedNodeSnapshot> nodes;
	HashMap<String, Vector<int>> native_id_index;
};

struct RNDesiredNode {
	String component_name;
	std::shared_ptr<class RNEventTarget> event_target;
};

enum class RNImperativeRequestKind {
	COMMAND,
	DIRECT_PROPS,
};

struct RNImperativeRequest {
	uint64_t runtime_generation = 0;
	int root_tag = 0;
	uint64_t surface_epoch = 0;
	uint64_t required_revision = 0;
	int tag = 0;
	String component_name;
	RNImperativeRequestKind kind = RNImperativeRequestKind::COMMAND;
	String command_name;
	Variant payload;
};

struct RNPendingCommit {
	uint64_t runtime_generation = 0;
	int root_tag = 0;
	uint64_t surface_epoch = 0;
	uint64_t revision = 0;
	Ref<RNShadowNode> tree;
};

class RNPointerCaptureProcessor {
	struct PointerKey {
		int root_tag = 0;
		int pointer_id = 0;

		bool operator==(const PointerKey &p_other) const {
			return root_tag == p_other.root_tag && pointer_id == p_other.pointer_id;
		}
	};

	struct PointerKeyHash {
		size_t operator()(const PointerKey &p_value) const {
			return (static_cast<size_t>(static_cast<uint32_t>(p_value.root_tag)) << 32) ^ static_cast<uint32_t>(p_value.pointer_id);
		}
	};

	struct PointerState {
		uint64_t surface_epoch = 0;
		int active_target = 0;
		int capture_target = 0;
		int pending_target = 0;
		Dictionary sample;
	};

	std::unordered_map<PointerKey, PointerState, PointerKeyHash> pointers;

public:
	void observe(const RNNativeEvent &p_event);
	Vector<RNNativeEvent> apply_pending(const RNNativeEvent &p_event);
	bool has_capture(int p_root_tag, uint64_t p_epoch, int p_tag, int p_pointer_id) const;
	void set_capture(int p_root_tag, uint64_t p_epoch, int p_tag, int p_pointer_id);
	void release_capture(int p_root_tag, uint64_t p_epoch, int p_tag, int p_pointer_id);
	int captured_target(int p_root_tag, uint64_t p_epoch, int p_pointer_id) const;
	void finish(int p_root_tag, uint64_t p_epoch, int p_pointer_id);
	Vector<RNNativeEvent> reconcile_surface(const RNSurfaceSnapshot &p_snapshot);
	Vector<RNNativeEvent> clear_surface(int p_root_tag, uint64_t p_epoch, uint64_t p_generation);
	void clear();
};

struct RNRuntimeCoordinatorState {
	std::unordered_map<int, RNSurfaceRoute> routes;
	std::unordered_map<uint64_t, String> registered_roots;
	std::unordered_map<uint64_t, int> root_tags_by_object_id;
	std::unordered_map<int, std::shared_ptr<const RNSurfaceSnapshot>> snapshots;
	std::unordered_map<RNSurfaceTag, RNDesiredNode, RNSurfaceTagHash> desired_nodes;
	std::deque<RNPendingCommit> commit_queue;
	std::deque<RNImperativeRequest> imperative_queue;
	std::deque<RNNativeEvent> event_queue;
	std::weak_ptr<FabricUIManager> ui_manager;
	RNPointerCaptureProcessor pointer_capture;
	RNBundleStatus bundle_status = RNBundleStatus::UNEVALUATED;
	uint64_t bundle_generation = 0;
	String bundle_error;
	int64_t next_root_tag = 11;
	uint64_t next_surface_epoch = 1;
	bool shutting_down = false;
};

class ReactNativeRuntimeCoordinator : public Object {
	GDCLASS(ReactNativeRuntimeCoordinator, Object);

	static ReactNativeRuntimeCoordinator *singleton;

	std::shared_ptr<RNRuntimeCoordinatorState> state;
	std::shared_ptr<FabricUIManager> ui_manager;
	std::shared_ptr<NativeDOM> native_dom;
	ObjectID connected_tree_id;
	bool frame_connected = false;

	int allocate_root_tag();
	RNSurfaceRoute *find_route(ObjectID p_root_id);
	bool ensure_bundle();
	void start_root(ReactNativeRootView *p_root);
	void stop_route(RNSurfaceRoute &p_route, bool p_dispatch_cancellations);
	void connect_frame_signal(ReactNativeRootView *p_root);
	void disconnect_frame_signal();
	void clear_generation_state();
	void _on_react_native_file_changed(const String &p_path, const String &p_content, bool p_exists);

protected:
	static void _bind_methods();

public:
	ReactNativeRuntimeCoordinator();
	~ReactNativeRuntimeCoordinator() override;

	static ReactNativeRuntimeCoordinator *get_singleton();
	std::shared_ptr<RNRuntimeCoordinatorState> get_state() const { return state; }
	std::shared_ptr<FabricUIManager> get_ui_manager() const { return ui_manager; }

	void register_root(ReactNativeRootView *p_root);
	void unregister_root(ReactNativeRootView *p_root);
	void reload_root(ReactNativeRootView *p_root);
	void application_key_changed(ReactNativeRootView *p_root);
	void enqueue_events(const Vector<RNNativeEvent> &p_events);
	void publish_snapshot(const std::shared_ptr<const RNSurfaceSnapshot> &p_snapshot);
	std::shared_ptr<const RNSurfaceSnapshot> get_snapshot(int p_root_tag) const;
	void fail_surface(int p_root_tag, uint64_t p_epoch, const String &p_error);
	void mark_surface_mounted(int p_root_tag, uint64_t p_epoch, uint64_t p_revision);
	void _process_frame();
};
