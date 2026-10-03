#include "rn_mounting_manager.h"

#include "../components/rn_host_descriptor_registry.h"
#include "../components/rn_visual_style.h"
#include "../fabric/fabric_ui_manager.h"
#include "../fabric/rn_view_style.h"
#include "../root_view/react_native_root_view.h"

#include "core/object/callable_mp.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"

namespace {

float number_prop(const Dictionary &p_props, const String &p_name, float p_default = 0.0f) {
	const Variant value = p_props.get(p_name, p_default);
	return value.get_type() == Variant::INT || value.get_type() == Variant::FLOAT ? float(value) : p_default;
}

Vector4 border_widths(const Dictionary &p_props, bool p_rtl) {
	const float all = number_prop(p_props, "borderWidth");
	const float left = number_prop(p_props, p_rtl ? "borderEndWidth" : "borderStartWidth", number_prop(p_props, "borderLeftWidth", all));
	const float right = number_prop(p_props, p_rtl ? "borderStartWidth" : "borderEndWidth", number_prop(p_props, "borderRightWidth", all));
	return Vector4(number_prop(p_props, "borderTopWidth", all), right, number_prop(p_props, "borderBottomWidth", all), left);
}

bool is_host(const Ref<RNShadowNode> &p_node) {
	return p_node.is_valid() && p_node->descriptor && p_node->descriptor->get_traits().creates_host;
}

int mutation_stat_index(RNMutationType p_type) {
	return int(p_type);
}

HashMap<int, Dictionary> copy_overrides(const HashMap<int, Dictionary> &p_source) {
	HashMap<int, Dictionary> result;
	result.reserve(p_source.size());
	for (const KeyValue<int, Dictionary> &entry : p_source) {
		result[entry.key] = entry.value.duplicate(true);
	}
	return result;
}

HashMap<int, RNMountedNode> copy_records(const HashMap<int, RNMountedNode> &p_source) {
	HashMap<int, RNMountedNode> result;
	result.reserve(p_source.size());
	for (const KeyValue<int, RNMountedNode> &entry : p_source) {
		result[entry.key] = entry.value;
	}
	return result;
}

} // namespace

RNMountingManager::RNMountingManager(ReactNativeRootView *p_owner) :
		owner(p_owner) {
}

RNMountingManager::~RNMountingManager() {
	clear(false);
}

Control *RNMountingManager::mount_container() const {
	return Object::cast_to<Control>(ObjectDB::get_instance(mount_container_id));
}

Control *RNMountingManager::host_for_tag(int p_tag) const {
	return Object::cast_to<Control>(registry.get_node(p_tag));
}

Control *RNMountingManager::native_parent(int p_tag) const {
	if (p_tag == root_tag) {
		return mount_container();
	}
	Control *host = host_for_tag(p_tag);
	Ref<RNShadowNode> shadow = registry.get_shadow_node(p_tag);
	return host && shadow.is_valid() && shadow->descriptor ? shadow->descriptor->get_child_container(host, host_context(p_tag, published_revision)) : nullptr;
}

int RNMountingManager::native_child_position(Control *p_parent, int p_index) const {
	int index = 0;
	for (int i = 0; i < p_parent->get_child_count(); ++i) {
		if (registry.get_tag(p_parent->get_child(i)->get_instance_id()) != 0 && index++ == p_index) {
			return i;
		}
	}
	return MAX(0, p_parent->get_child_count() - 1);
}

void RNMountingManager::ensure_mount_container() {
	if (mount_container() || !owner) {
		return;
	}
	Control *container = memnew(Control);
	container->set_name("ReactNativeMountContainer");
	container->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	container->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	owner->add_child(container);
	mount_container_id = container->get_instance_id();
}

void RNMountingManager::attach(uint64_t p_generation, int p_root_tag, uint64_t p_surface_epoch) {
	clear(true);
	runtime_generation = p_generation;
	root_tag = p_root_tag;
	surface_epoch = p_surface_epoch;
	published_revision = 0;
	ensure_mount_container();
}

void RNMountingManager::clear(bool p_keep_container) {
	transaction_in_flight = true;
	for (const KeyValue<int, RNMountedNode> &entry : mounted_nodes) {
		if (!entry.value.renderer_owned) {
			continue;
		}
		Control *host = Object::cast_to<Control>(ObjectDB::get_instance(entry.value.object_id));
		if (host && host->get_parent()) {
			host->get_parent()->remove_child(host);
		}
	}
	for (const KeyValue<int, RNMountedNode> &entry : mounted_nodes) {
		if (!entry.value.renderer_owned) {
			continue;
		}
		Control *host = Object::cast_to<Control>(ObjectDB::get_instance(entry.value.object_id));
		if (host) {
			if (entry.value.descriptor) {
				entry.value.descriptor->detach_signals(host, host_context(entry.key, published_revision));
				entry.value.descriptor->dispose_state(host, host_context(entry.key, published_revision));
			}
			host->queue_free();
			stats.hosts_freed++;
		}
	}
	registry.clear();
	mounted_nodes.clear();
	direct_prop_overrides.clear();
	declarative_root.unref();
	committed_root.unref();
	published_snapshot.reset();
	prepared_hosts.clear();
	layout_tree.clear();
	published_revision = 0;
	if (!p_keep_container) {
		Control *container = mount_container();
		if (container) {
			if (container->get_parent()) {
				container->get_parent()->remove_child(container);
			}
			container->queue_free();
		}
		mount_container_id = ObjectID();
	}
	transaction_in_flight = false;
	publication_pending = false;
}

Ref<RNShadowNode> RNMountingManager::build_effective_tree(const Ref<RNShadowNode> &p_node, const HashMap<int, Dictionary> &p_overrides) const {
	if (p_node.is_null()) {
		return Ref<RNShadowNode>();
	}
	const Dictionary *overrides = p_overrides.getptr(p_node->tag);
	Vector<Ref<RNShadowNode>> effective_children;
	bool child_changed = false;
	for (const Ref<RNShadowNode> &child : p_node->children) {
		Ref<RNShadowNode> effective = build_effective_tree(child, p_overrides);
		effective_children.push_back(effective);
		child_changed = child_changed || effective.ptr() != child.ptr();
	}
	if (!overrides && !child_changed) {
		return p_node;
	}
	Ref<RNShadowNode> result = p_node->clone(true, nullptr);
	if (overrides) {
		const Array keys = overrides->keys();
		for (int i = 0; i < keys.size(); ++i) {
			result->props[keys[i]] = (*overrides)[keys[i]];
		}
	}
	result->children = effective_children;
	return result;
}

void RNMountingManager::reconcile_overrides(const Ref<RNShadowNode> &p_node, HashMap<int, Dictionary> &r_overrides, HashSet<int> &r_live_tags) const {
	if (p_node.is_null()) {
		return;
	}
	r_live_tags.insert(p_node->tag);
	Dictionary *overrides = r_overrides.getptr(p_node->tag);
	if (overrides) {
		const RNMountedNode *mounted = mounted_nodes.getptr(p_node->tag);
		for (const KeyValue<String, uint64_t> &entry : p_node->declarative_prop_revisions) {
			const uint64_t *published = mounted ? mounted->shadow_node->declarative_prop_revisions.getptr(entry.key) : nullptr;
			if (!published || *published != entry.value) {
				overrides->erase(entry.key);
			}
		}
		if (overrides->is_empty()) {
			r_overrides.erase(p_node->tag);
		}
	}
	for (const Ref<RNShadowNode> &child : p_node->children) {
		reconcile_overrides(child, r_overrides, r_live_tags);
	}
}

RNHostContext RNMountingManager::host_context(int p_tag, uint64_t p_revision) const {
	RNHostContext context;
	context.owner = owner;
	context.host_id = host_for_tag(p_tag) ? host_for_tag(p_tag)->get_instance_id() : ObjectID();
	context.generation = runtime_generation;
	context.root_tag = root_tag;
	context.surface_epoch = surface_epoch;
	context.revision = p_revision;
	context.tag = p_tag;
	if (owner) {
		const ObjectID owner_id = owner->get_instance_id();
		const ObjectID host_id = host_for_tag(p_tag) ? host_for_tag(p_tag)->get_instance_id() : ObjectID();
		const uint64_t generation = runtime_generation;
		const uint64_t epoch = surface_epoch;
		auto sink = std::make_shared<RNHostEventSink>();
		sink->emit = [owner_id, host_id, generation, epoch](int p_target, const StringName &p_name, const Dictionary &p_payload, uint64_t p_owner_revision) {
			ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(owner_id));
			if (root) {
				root->_emit_host_event(generation, epoch, p_target, host_id, p_name, p_payload, p_owner_revision);
			}
		};
		sink->invalidate_geometry = [owner_id, generation, epoch]() {
			ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(owner_id));
			if (root && root->runtime_generation == generation && root->surface_epoch == epoch) {
				root->_invalidate_host_geometry();
			}
		};
		sink->invalidate_layout = [owner_id, generation, epoch]() {
			ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(owner_id));
			if (root && root->runtime_generation == generation && root->surface_epoch == epoch) {
				root->_invalidate_host_layout();
			}
		};
		sink->cancel_input = [owner_id, generation, epoch]() {
			ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(owner_id));
			if (root && root->runtime_generation == generation && root->surface_epoch == epoch) {
				root->_cancel_host_input();
			}
		};
		sink->is_current = [owner_id, host_id, generation, epoch, p_tag, p_revision]() {
			ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(owner_id));
			return root && root->runtime_generation == generation && root->surface_epoch == epoch && root->mounting_manager->is_current_host(p_tag, host_id, p_revision);
		};
		context.event_sink = sink;
	}
	return context;
}

Control *RNMountingManager::create_host(const Ref<RNShadowNode> &p_node, const RNHostContext &p_context) {
	return p_node.is_valid() && p_node->descriptor ? p_node->descriptor->create_host(p_context) : nullptr;
}

bool RNMountingManager::apply_host_props(Control *p_host, const std::shared_ptr<const RNHostDescriptor> &p_descriptor, const RNPreparedHostState &p_state, const RNHostContext &p_context, String &r_error) {
	if (!p_host || !p_descriptor) {
		r_error = "host or descriptor is missing";
		return false;
	}
	p_host->set_meta("react_native_test_id", p_state.props.get("testID", String()));
	RNError error;
	if (!p_descriptor->apply(p_host, p_state, p_context, error)) {
		r_error = error.describe();
		return false;
	}
	return true;
}

void RNMountingManager::apply_layout(Control *p_host, const Rect2 &p_layout) {
	if (!p_host) {
		return;
	}
	p_host->set_external_layout_enabled(true);
	p_host->set_position(p_layout.position);
	p_host->set_size(p_layout.size);
	stats.layout_boxes_applied++;
}

bool RNMountingManager::prepare_transaction(RNMountingTransaction &r_transaction, const Ref<RNShadowNode> &p_next_root, const Size2 &p_constraint, String &r_error) {
	if (p_next_root.is_null() || p_next_root->tag != root_tag || p_next_root->root_tag != root_tag || p_next_root->runtime_generation != runtime_generation || p_next_root->surface_epoch != surface_epoch) {
		r_error = vformat("surface %d revision %d has stale or invalid root metadata", root_tag, r_transaction.revision);
		return false;
	}
	if (!RNShadowNode::is_within_depth_limit(p_next_root)) {
		r_error = vformat("surface %d revision %d shadow tree exceeds RNShadowNode::MAX_DEPTH", root_tag, r_transaction.revision);
		return false;
	}
	if (!hierarchy_matches(mounted_nodes)) {
		r_error = vformat("surface %d has an expired host or invalid native index", root_tag);
		return false;
	}
	std::shared_ptr<RNHostDescriptorRegistry> descriptors;
	if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
		descriptors = coordinator->get_descriptor_registry();
	}
	if (!descriptors) {
		r_error = "host descriptor registry is unavailable";
		return false;
	}
	HashMap<int, Size2> presentations;
	presentations[p_next_root->tag] = owner && owner->is_inside_tree() ? Size2(owner->get_window()->get_size()) : p_constraint;
	HashMap<int, bool> directions;
	directions[p_next_root->tag] = owner && owner->is_layout_rtl();
	Vector<Ref<RNShadowNode>> pending;
	pending.push_back(p_next_root);
	while (!pending.is_empty()) {
		Ref<RNShadowNode> node = pending[pending.size() - 1];
		pending.remove_at(pending.size() - 1);
		if (node.is_null()) {
			continue;
		}
		if (!node->descriptor || node->descriptor->get_name() != StringName(node->view_name)) {
			node->descriptor = descriptors->find(node->view_name);
		}
		if (!node->descriptor) {
			r_error = vformat("E_UNKNOWN_COMPONENT: no host descriptor is registered for '%s' (tag %d)", node->view_name, node->tag);
			return false;
		}
		RNPreparedHostState prepared;
		RNError prepare_error;
		const String direction = node->props.get("direction", "inherit");
		RNHostContext context = host_context(node->tag, r_transaction.revision);
		context.presentation_size = presentations[node->tag];
		if (!node->descriptor->prepare(*node.ptr(), prepared, prepare_error)) {
			r_error = prepare_error.describe();
			return false;
		}
		prepared.layout_rtl = direction == "rtl" || (direction != "ltr" && directions[node->tag]);
		if (!node->descriptor->resolve_resources(prepared, context, prepare_error)) {
			r_error = prepare_error.describe();
			return false;
		}
		prepared.dependency_revision ^= prepared.layout_rtl ? 0x9e3779b97f4a7c15ULL : 0;
		r_transaction.prepared_states[node->tag] = prepared;
		for (const Ref<RNShadowNode> &child : node->children) {
			if (child.is_null()) {
				continue;
			}
			directions[child->tag] = prepared.layout_rtl;
			presentations[child->tag] = node->descriptor->presentation_size(prepared, presentations[node->tag]);
			pending.push_back(child);
		}
	}
	r_transaction.old_root = committed_root;
	r_transaction.new_root = p_next_root;
	stats.differ = RNTreeDifferStats();
	if (!RNTreeDiffer::diff(committed_root, p_next_root, r_transaction.mutations, r_error, &stats.differ)) {
		r_error = vformat("surface %d revision %d: %s", root_tag, r_transaction.revision, r_error);
		return false;
	}
	if (!layout_tree.prepare(p_next_root, p_constraint, r_transaction.prepared_layouts, r_error, &r_transaction.prepared_states)) {
		return false;
	}
	for (const KeyValue<int, Rect2> &entry : r_transaction.prepared_layouts) {
		const RNPreparedHostState *state = r_transaction.prepared_states.getptr(entry.key);
		if (!entry.value.is_finite() || (state && !RNVisualStyle::transform(state->props, entry.value.size).is_finite())) {
			r_error = "Resolved host geometry must be finite";
			return false;
		}
	}
	prepared_hosts.clear();
	for (const RNMountingMutation &mutation : r_transaction.mutations) {
		if (mutation.type != RNMutationType::CREATE) {
			continue;
		}
		Control *host = create_host(mutation.new_node, host_context(mutation.new_node->tag, r_transaction.revision));
		if (!host) {
			r_error = vformat("failed to create host for tag %d", mutation.new_node->tag);
			destroy_detached_hosts(r_transaction);
			return false;
		}
		prepared_hosts[mutation.new_node->tag] = host->get_instance_id();
		r_transaction.prepared_host_descriptors[mutation.new_node->tag] = mutation.new_node->descriptor;
	}
	return true;
}

void RNMountingManager::destroy_detached_hosts(RNMountingTransaction &p_transaction) {
	for (const KeyValue<int, ObjectID> &entry : prepared_hosts) {
		Control *host = Object::cast_to<Control>(ObjectDB::get_instance(entry.value));
		if (!host) {
			continue;
		}
		if (const std::shared_ptr<const RNHostDescriptor> *descriptor = p_transaction.prepared_host_descriptors.getptr(entry.key)) {
			(*descriptor)->dispose_state(host, host_context(entry.key, p_transaction.revision));
		}
		if (host->get_parent()) {
			host->get_parent()->remove_child(host);
		}
		memdelete(host);
	}
	p_transaction.prepared_host_descriptors.clear();
	prepared_hosts.clear();
}

bool RNMountingManager::build_snapshot_node(const Ref<RNShadowNode> &p_node, int p_logical_parent, int p_native_parent, const Point2 &p_parent_origin, bool p_branch_targetable, const HashMap<int, Dictionary> &p_overrides, const HashMap<int, Rect2> &p_layouts, const HashMap<int, RNPreparedHostState> &p_prepared_states, const Transform2D &p_window_transform, uint64_t p_revision, RNSurfaceSnapshot &r_snapshot, HashMap<int, RNMountedNode> &r_records, HashMap<int, int> &r_native_indices, String &r_error) const {
	if (p_node.is_null()) {
		return true;
	}
	const RNPreparedHostState *prepared_state = p_prepared_states.getptr(p_node->tag);
	if (!prepared_state) {
		r_error = vformat("missing prepared descriptor state for tag %d", p_node->tag);
		return false;
	}
	const bool host = is_host(p_node);
	const Rect2 *prepared_layout = p_layouts.getptr(p_node->tag);
	const Rect2 layout = prepared_layout ? *prepared_layout : Rect2();
	const String pointer_events = String(p_node->props.get("pointerEvents", "auto")).to_lower();
	const bool branch_enabled = p_branch_targetable && pointer_events != "none";
	const bool text_target = (p_node->view_name != "RCTText" && p_node->view_name != "RCTVirtualText") || bool(p_node->props.get("isPressable", false)) || bool(p_node->props.get("selectable", false));
	const bool self_targetable = text_target && (host || p_node->view_name == "RCTVirtualText") && p_node->descriptor->get_traits().input_target && branch_enabled && pointer_events != "box-none";
	const bool descendants_targetable = branch_enabled && pointer_events != "box-only";
	RNMountedNode mounted;
	mounted.logical_parent_tag = p_logical_parent;
	mounted.native_parent_tag = host ? p_native_parent : 0;
	mounted.native_index = host ? r_native_indices[p_native_parent]++ : -1;
	mounted.object_id = host ? (prepared_hosts.has(p_node->tag) ? *prepared_hosts.getptr(p_node->tag) : registry.get_node(p_node->tag) ? registry.get_node(p_node->tag)->get_instance_id()
																																	   : ObjectID())
							 : ObjectID();
	mounted.view_name = p_node->view_name;
	mounted.descriptor = p_node->descriptor;
	mounted.shadow_node = p_node;
	mounted.prepared_state = *prepared_state;
	mounted.prepared_state.branch_targetable = p_branch_targetable;
	mounted.declarative_props = p_node->props.duplicate(true);
	if (const Dictionary *overrides = p_overrides.getptr(p_node->tag)) {
		mounted.direct_prop_overrides = overrides->duplicate(true);
	}
	mounted.last_changed_revision = p_revision;
	mounted.renderer_owned = host;
	RNMountedNodeSnapshot &snapshot = mounted.snapshot;
	snapshot.tag = p_node->tag;
	snapshot.parent_tag = p_logical_parent;
	snapshot.view_name = p_node->view_name;
	snapshot.native_id = String(p_node->props.get("nativeID", String()));
	snapshot.text_content = p_node->collect_text();
	snapshot.local_rect = layout;
	snapshot.root_rect = Rect2(p_parent_origin + layout.position, layout.size);
	snapshot.window_rect = p_window_transform.xform(snapshot.root_rect);
	snapshot.border_width = border_widths(prepared_state->props, prepared_state->layout_rtl);
	snapshot.inner_size = Size2(MAX(0.0f, snapshot.root_rect.size.x - snapshot.border_width.y - snapshot.border_width.w), MAX(0.0f, snapshot.root_rect.size.y - snapshot.border_width.x - snapshot.border_width.z));
	snapshot.offset_parent_tag = p_logical_parent;
	snapshot.offset = layout.position;
	if (const RNMountedNodeSnapshot *parent = r_snapshot.nodes.getptr(p_logical_parent)) {
		snapshot.offset -= Point2(parent->border_width.w, parent->border_width.x);
	}
	snapshot.object_id = mounted.object_id;
	snapshot.shadow_node = p_node;
	snapshot.pointer_events = pointer_events;
	snapshot.hit_slop = p_node->props.get("hitSlop", Variant());
	snapshot.branch_targetable = descendants_targetable;
	snapshot.self_targetable = self_targetable;
	snapshot.visible = String(p_node->props.get("display", "flex")) != "none";
	snapshot.clips_contents = String(p_node->props.get("overflow", "visible")) == "hidden";
	for (const Ref<RNShadowNode> &child : p_node->children) {
		if (child.is_null()) {
			continue;
		}
		snapshot.child_tags.push_back(child->tag);
		if (is_host(child)) {
			snapshot.native_child_tags.push_back(child->tag);
		}
	}
	r_snapshot.nodes[p_node->tag] = snapshot;
	if (!snapshot.native_id.is_empty()) {
		r_snapshot.native_id_index[snapshot.native_id].push_back(snapshot.tag);
	}
	r_records[p_node->tag] = mounted;
	const bool transparent_container = !host && p_node->descriptor->get_traits().has_native_children;
	const int child_native_parent = transparent_container ? p_node->tag : host ? p_node->tag
																			   : p_native_parent;
	const Point2 child_origin = p_node->descriptor->get_traits().contributes_text ? p_parent_origin : snapshot.root_rect.position;
	for (const Ref<RNShadowNode> &child : p_node->children) {
		if (!build_snapshot_node(child, p_node->tag, child_native_parent, child_origin, descendants_targetable, p_overrides, p_layouts, p_prepared_states, p_window_transform, p_revision, r_snapshot, r_records, r_native_indices, r_error)) {
			return false;
		}
	}
	return true;
}

std::shared_ptr<RNSurfaceSnapshot> RNMountingManager::build_snapshot(const Ref<RNShadowNode> &p_root, const Ref<RNShadowNode> &p_declarative_root, const HashMap<int, Dictionary> &p_overrides, const HashMap<int, Rect2> &p_layouts, const HashMap<int, RNPreparedHostState> &p_prepared_states, const Transform2D &p_window_transform, uint64_t p_revision, HashMap<int, RNMountedNode> &r_records, String &r_error) const {
	auto snapshot = std::make_shared<RNSurfaceSnapshot>();
	snapshot->root_tag = root_tag;
	snapshot->runtime_generation = runtime_generation;
	snapshot->surface_epoch = surface_epoch;
	snapshot->revision = p_revision;
	HashMap<int, int> native_indices;
	if (!build_snapshot_node(p_root, 0, root_tag, Point2(), true, p_overrides, p_layouts, p_prepared_states, p_window_transform, p_revision, *snapshot, r_records, native_indices, r_error)) {
		return nullptr;
	}
	Vector<Ref<RNShadowNode>> pending;
	pending.push_back(p_declarative_root);
	while (!pending.is_empty()) {
		Ref<RNShadowNode> node = pending[pending.size() - 1];
		pending.remove_at(pending.size() - 1);
		if (node.is_null()) {
			continue;
		}
		if (RNMountedNode *mounted = r_records.getptr(node->tag)) {
			mounted->declarative_props = node->props.duplicate(true);
		}
		for (const Ref<RNShadowNode> &child : node->children) {
			pending.push_back(child);
		}
	}
	return snapshot;
}

bool RNMountingManager::hierarchy_matches(const HashMap<int, RNMountedNode> &p_records) const {
	for (const KeyValue<int, RNMountedNode> &entry : p_records) {
		const RNMountedNode &mounted = entry.value;
		if (!mounted.renderer_owned) {
			continue;
		}
		Control *host = Object::cast_to<Control>(ObjectDB::get_instance(mounted.object_id));
		Control *parent = native_parent(mounted.native_parent_tag);
		if (!host || !parent || registry.get_tag(mounted.object_id) != entry.key || mounted.shadow_node.is_null() || mounted.shadow_node->root_tag != root_tag || mounted.shadow_node->runtime_generation != runtime_generation || mounted.shadow_node->surface_epoch != surface_epoch || host->get_parent() != parent) {
			return false;
		}
		int index = 0;
		for (int i = 0; i < host->get_index(); ++i) {
			index += registry.get_tag(parent->get_child(i)->get_instance_id()) != 0;
		}
		if (index != mounted.native_index) {
			return false;
		}
	}
	return true;
}

void RNMountingManager::queue_layout_events(const std::shared_ptr<const RNSurfaceSnapshot> &p_old_snapshot, const RNSurfaceSnapshot &p_new_snapshot, Vector<RNNativeEvent> &r_events) const {
	for (const KeyValue<int, RNMountedNodeSnapshot> &entry : p_new_snapshot.nodes) {
		const RNMountedNodeSnapshot &node = entry.value;
		if (!node.shadow_node->descriptor || !node.shadow_node->descriptor->get_traits().emits_layout) {
			continue;
		}
		const RNMountedNodeSnapshot *old = p_old_snapshot ? p_old_snapshot->nodes.getptr(entry.key) : nullptr;
		if (!node.shadow_node->props.has("onLayout") || (old && old->local_rect.is_equal_approx(node.local_rect))) {
			continue;
		}
		Dictionary rectangle;
		rectangle["x"] = node.local_rect.position.x;
		rectangle["y"] = node.local_rect.position.y;
		rectangle["width"] = node.local_rect.size.x;
		rectangle["height"] = node.local_rect.size.y;
		RNNativeEvent event;
		event.tag = node.tag;
		event.name = "topLayout";
		event.priority = FabricUIManager::EVENT_PRIORITY_DEFAULT;
		event.payload["layout"] = rectangle;
		r_events.push_back(event);
	}
}

void RNMountingManager::restore_scene(const HashMap<int, RNMountedNode> &p_records, const Ref<RNShadowNode> &p_root, const Size2 &p_constraint, const RNMountingTransaction &p_transaction) {
	for (const KeyValue<int, ObjectID> &entry : prepared_hosts) {
		Control *host = Object::cast_to<Control>(ObjectDB::get_instance(entry.value));
		if (!host) {
			continue;
		}
		if (const std::shared_ptr<const RNHostDescriptor> *descriptor = p_transaction.prepared_host_descriptors.getptr(entry.key)) {
			(*descriptor)->dispose_state(host, host_context(entry.key, p_transaction.revision));
		}
		if (host->get_parent()) {
			host->get_parent()->remove_child(host);
		}
		if (registry.get_tag(entry.value) == entry.key) {
			stats.hosts_freed++;
		}
		registry.unregister_node(entry.key);
		memdelete(host);
	}
	prepared_hosts.clear();
	Vector<const RNMountedNode *> ordered;
	for (const KeyValue<int, RNMountedNode> &entry : p_records) {
		if (entry.value.renderer_owned) {
			ordered.push_back(&entry.value);
		}
	}
	for (int i = 1; i < ordered.size(); ++i) {
		int current = i;
		while (current > 0) {
			const RNMountedNode *left = ordered[current];
			const RNMountedNode *right = ordered[current - 1];
			auto depth_of = [&](const RNMountedNode *p_node) {
				int depth = 0;
				int parent_tag = p_node->native_parent_tag;
				while (parent_tag != root_tag) {
					const RNMountedNode *parent = p_records.getptr(parent_tag);
					if (!parent || ++depth >= RNShadowNode::MAX_DEPTH) {
						break;
					}
					parent_tag = parent->native_parent_tag;
				}
				return depth;
			};
			const int left_depth = depth_of(left);
			const int right_depth = depth_of(right);
			const bool before = left_depth == right_depth ? left->native_parent_tag == right->native_parent_tag && left->native_index < right->native_index : left_depth < right_depth;
			if (!before) {
				break;
			}
			std::swap(ordered.write[current], ordered.write[current - 1]);
			current--;
		}
	}
	for (const RNMountedNode *mounted : ordered) {
		Control *host = Object::cast_to<Control>(ObjectDB::get_instance(mounted->object_id));
		Control *parent = native_parent(mounted->native_parent_tag);
		if (host && parent && host->get_parent() != parent) {
			if (host->get_parent()) {
				host->get_parent()->remove_child(host);
			}
			parent->add_child(host);
		}
		if (host && parent) {
			parent->move_child(host, native_child_position(parent, mounted->native_index));
			String ignored_apply_error;
			apply_host_props(host, mounted->descriptor, mounted->prepared_state, host_context(mounted->snapshot.tag, mounted->last_changed_revision), ignored_apply_error);
			apply_layout(host, mounted->snapshot.local_rect);
			RNVisualStyle::apply(host, mounted->prepared_state.props);
			host->set_layout_direction(mounted->prepared_state.layout_rtl ? Control::LAYOUT_DIRECTION_RTL : Control::LAYOUT_DIRECTION_LTR);
			registry.register_node(mounted->snapshot.tag, host, mounted->shadow_node);
		}
	}
	if (owner && owner->focused_tag != 0) {
		Control *focused = Object::cast_to<Control>(registry.get_node(owner->focused_tag));
		if (focused && (focused->get_focus_mode() == Control::FOCUS_CLICK || focused->get_focus_mode() == Control::FOCUS_ALL) && !focused->get_viewport()->gui_get_focus_owner()) {
			focused->grab_focus();
		}
	}
	String ignored;
	HashMap<int, RNPreparedHostState> prepared_states;
	for (const KeyValue<int, RNMountedNode> &entry : p_records) {
		prepared_states[entry.key] = entry.value.prepared_state;
	}
	layout_tree.rebuild(p_root, p_constraint, ignored, &prepared_states);
}

bool RNMountingManager::apply_transaction(RNMountingTransaction &p_transaction, const Ref<RNShadowNode> &p_declarative_root, const HashMap<int, Dictionary> &p_next_overrides, const Size2 &p_constraint, const Transform2D &p_window_transform, Vector<RNNativeEvent> &r_events, String &r_error) {
	const HashMap<int, RNMountedNode> old_records = copy_records(mounted_nodes);
	const Ref<RNShadowNode> old_root = committed_root;
	const std::shared_ptr<const RNSurfaceSnapshot> old_snapshot = published_snapshot;
	transaction_in_flight = true;
	for (const KeyValue<int, RNMountedNode> &entry : old_records) {
		Control *host = Object::cast_to<Control>(ObjectDB::get_instance(entry.value.object_id));
		if (host && entry.value.descriptor) {
			p_transaction.captured_native_states[entry.key] = entry.value.descriptor->capture_state(host);
		}
	}
	int applied_mutations = 0;
	int failed_mutation = -1;
	auto should_fail_before = [&]() {
		const bool should_fail = fail_before_mutation == applied_mutations;
		if (should_fail) {
			failed_mutation = applied_mutations;
		}
		return should_fail;
	};
	auto finish_mutation = [&]() {
		const bool fail = fail_after_mutation == applied_mutations;
		if (fail) {
			failed_mutation = applied_mutations;
		}
		applied_mutations++;
		return fail;
	};
	auto fail = [&](const RNMountingMutation &p_mutation) {
		const int mutation_index = failed_mutation >= 0 ? failed_mutation : applied_mutations;
		r_error = vformat("surface %d revision %d failed at mutation %d (%s)", root_tag, p_transaction.revision, mutation_index, rn_mutation_type_name(p_mutation.type));
		restore_scene(old_records, old_root, p_constraint, p_transaction);
		for (const KeyValue<int, Variant> &entry : p_transaction.captured_native_states) {
			const RNMountedNode *mounted = old_records.getptr(entry.key);
			Control *host = mounted ? Object::cast_to<Control>(ObjectDB::get_instance(mounted->object_id)) : nullptr;
			if (host && mounted->descriptor) {
				mounted->descriptor->restore_state(host, entry.value);
			}
		}
		stats.rollbacks++;
		transaction_in_flight = false;
		return false;
	};
	for (const RNMountingMutation &mutation : p_transaction.mutations) {
		if (mutation.type != RNMutationType::DELETE) {
			continue;
		}
		if (should_fail_before()) {
			return fail(mutation);
		}
		stats.mutations[mutation_stat_index(mutation.type)]++;
		if (finish_mutation()) {
			return fail(mutation);
		}
	}

	for (const RNMountingMutation &mutation : p_transaction.mutations) {
		if (mutation.type != RNMutationType::REMOVE) {
			continue;
		}
		if (should_fail_before()) {
			return fail(mutation);
		}
		Control *host = host_for_tag(mutation.old_node->tag);
		Control *parent = native_parent(mutation.parent_tag);
		if (!host || !parent || host->get_parent() != parent) {
			return fail(mutation);
		}
		parent->remove_child(host);
		stats.mutations[mutation_stat_index(mutation.type)]++;
		if (finish_mutation()) {
			return fail(mutation);
		}
	}
	for (const RNMountingMutation &mutation : p_transaction.mutations) {
		if (mutation.type != RNMutationType::CREATE) {
			continue;
		}
		if (should_fail_before()) {
			return fail(mutation);
		}
		ObjectID *id = prepared_hosts.getptr(mutation.new_node->tag);
		Control *host = id ? Object::cast_to<Control>(ObjectDB::get_instance(*id)) : nullptr;
		if (!host) {
			return fail(mutation);
		}
		registry.register_node(mutation.new_node->tag, host, mutation.new_node);
		stats.hosts_created++;
		stats.mutations[mutation_stat_index(mutation.type)]++;
		if (finish_mutation()) {
			return fail(mutation);
		}
	}
	for (const RNMountingMutation &mutation : p_transaction.mutations) {
		if (mutation.type != RNMutationType::INSERT) {
			continue;
		}
		if (should_fail_before()) {
			return fail(mutation);
		}
		Control *host = host_for_tag(mutation.new_node->tag);
		Control *parent = native_parent(mutation.parent_tag);
		if (!host || !parent || host->get_parent()) {
			return fail(mutation);
		}
		parent->add_child(host);
		parent->move_child(host, native_child_position(parent, mutation.index));
		stats.mutations[mutation_stat_index(mutation.type)]++;
		if (finish_mutation()) {
			return fail(mutation);
		}
	}

	HashMap<int, RNMountedNode> next_records;
	std::shared_ptr<RNSurfaceSnapshot> next_snapshot = build_snapshot(p_transaction.new_root, p_declarative_root, p_next_overrides, p_transaction.prepared_layouts, p_transaction.prepared_states, p_window_transform, p_transaction.revision, next_records, r_error);
	if (!next_snapshot) {
		RNMountingMutation failed;
		failed.type = RNMutationType::UPDATE;
		return fail(failed);
	}
	HashSet<int> changed_tags;
	for (const RNMountingMutation &mutation : p_transaction.mutations) {
		if (mutation.type == RNMutationType::CREATE || mutation.type == RNMutationType::UPDATE) {
			changed_tags.insert(mutation.new_node->tag);
		}
	}
	for (const RNMountingMutation &mutation : p_transaction.mutations) {
		if (mutation.type != RNMutationType::UPDATE) {
			continue;
		}
		if (should_fail_before()) {
			return fail(mutation);
		}
		stats.mutations[mutation_stat_index(mutation.type)]++;
		if (finish_mutation()) {
			return fail(mutation);
		}
	}
	for (KeyValue<int, RNMountedNode> &entry : next_records) {
		if (!entry.value.renderer_owned) {
			continue;
		}
		Control *host = Object::cast_to<Control>(ObjectDB::get_instance(entry.value.object_id));
		const RNMountedNode *old = old_records.getptr(entry.key);
		if (old && !changed_tags.has(entry.key) && old->prepared_state.dependency_revision == entry.value.prepared_state.dependency_revision) {
			entry.value.last_changed_revision = old->last_changed_revision;
		}
		if (changed_tags.has(entry.key) || (old && old->prepared_state.dependency_revision != entry.value.prepared_state.dependency_revision)) {
			if (old && old->descriptor && !p_transaction.captured_native_states.has(entry.key)) {
				p_transaction.captured_native_states[entry.key] = old->descriptor->capture_state(host);
			}
			String apply_error;
			if (!apply_host_props(host, entry.value.descriptor, entry.value.prepared_state, host_context(entry.key, p_transaction.revision), apply_error)) {
				r_error = apply_error;
				RNMountingMutation failed;
				failed.type = RNMutationType::UPDATE;
				return fail(failed);
			}
		}
		if (!old || !old->snapshot.local_rect.is_equal_approx(entry.value.snapshot.local_rect)) {
			apply_layout(host, entry.value.snapshot.local_rect);
		}
		RNVisualStyle::apply(host, entry.value.prepared_state.props);
		host->set_layout_direction(entry.value.prepared_state.layout_rtl ? Control::LAYOUT_DIRECTION_RTL : Control::LAYOUT_DIRECTION_LTR);
		registry.register_node(entry.key, host, entry.value.shadow_node);
	}
	sort_paint_order(next_records);
	if (!hierarchy_matches(next_records)) {
		RNMountingMutation mismatch;
		mismatch.type = RNMutationType::UPDATE;
		return fail(mismatch);
	}

	direct_prop_overrides = copy_overrides(p_next_overrides);
	next_snapshot->native_visual_revision = old_snapshot ? old_snapshot->native_visual_revision + (p_transaction.revision == published_revision ? 1 : 0) : 0;
	refresh_geometry(*next_snapshot, p_window_transform);
	mounted_nodes = std::move(next_records);
	committed_root = p_transaction.new_root;
	published_revision = p_transaction.revision;
	published_snapshot = next_snapshot;
	layout_tree.publish();
	for (const RNMountingMutation &mutation : p_transaction.mutations) {
		if (mutation.type != RNMutationType::CREATE) {
			continue;
		}
		Control *host = host_for_tag(mutation.new_node->tag);
		if (host && mutation.new_node->descriptor) {
			mutation.new_node->descriptor->attach_signals(host, host_context(mutation.new_node->tag, p_transaction.revision));
		}
	}
	queue_layout_events(old_snapshot, *next_snapshot, r_events);
	for (const RNMountingMutation &mutation : p_transaction.mutations) {
		if (mutation.type != RNMutationType::DELETE || mutation.old_node.is_null()) {
			continue;
		}
		const RNMountedNode *old = old_records.getptr(mutation.old_node->tag);
		if (!old || !old->renderer_owned || old->snapshot.tag != mutation.old_node->tag || old->shadow_node.is_null() || old->shadow_node->root_tag != root_tag || old->shadow_node->runtime_generation != runtime_generation || old->shadow_node->surface_epoch != surface_epoch || registry.get_tag(old->object_id) != mutation.old_node->tag) {
			continue;
		}
		Control *host = Object::cast_to<Control>(ObjectDB::get_instance(old->object_id));
		registry.unregister_node(mutation.old_node->tag);
		if (host) {
			if (old->descriptor) {
				old->descriptor->detach_signals(host, host_context(mutation.old_node->tag, p_transaction.revision));
				old->descriptor->dispose_state(host, host_context(mutation.old_node->tag, p_transaction.revision));
			}
			if (host->get_parent()) {
				host->get_parent()->remove_child(host);
			}
			host->queue_free();
			stats.hosts_freed++;
		}
	}
	p_transaction.prepared_host_descriptors.clear();
	prepared_hosts.clear();
	transaction_in_flight = false;
	publication_pending = true;
	return true;
}

void RNMountingManager::activate_published_hosts() {
	if (!publication_pending || transaction_in_flight) {
		return;
	}
	publication_pending = false;
	Vector<Ref<RNShadowNode>> pending;
	pending.push_back(committed_root);
	while (!pending.is_empty()) {
		Ref<RNShadowNode> node = pending[pending.size() - 1];
		pending.remove_at(pending.size() - 1);
		if (node.is_null()) {
			continue;
		}
		const RNMountedNode *entry = mounted_nodes.getptr(node->tag);
		Control *host = entry ? Object::cast_to<Control>(ObjectDB::get_instance(entry->object_id)) : nullptr;
		if (host && entry->descriptor) {
			entry->descriptor->after_publish(host, entry->prepared_state, host_context(node->tag, entry->last_changed_revision));
		}
		for (int i = node->children.size() - 1; i >= 0; --i) {
			pending.push_back(node->children[i]);
		}
	}
}

bool RNMountingManager::commit(const RNPendingCommit &p_commit, const Size2 &p_constraint, const Transform2D &p_window_transform, Vector<RNNativeEvent> &r_events, String &r_error) {
	if (transaction_in_flight || p_commit.runtime_generation != runtime_generation || p_commit.root_tag != root_tag || p_commit.surface_epoch != surface_epoch) {
		r_error = "stale or reentrant mounting transaction";
		return false;
	}
	if (p_commit.revision <= published_revision) {
		return true;
	}
	HashMap<int, Dictionary> next_overrides = copy_overrides(direct_prop_overrides);
	HashSet<int> live_tags;
	reconcile_overrides(p_commit.tree, next_overrides, live_tags);
	Vector<int> stale_override_tags;
	for (const KeyValue<int, Dictionary> &entry : next_overrides) {
		if (!live_tags.has(entry.key)) {
			stale_override_tags.push_back(entry.key);
		}
	}
	for (int tag : stale_override_tags) {
		next_overrides.erase(tag);
	}
	Ref<RNShadowNode> effective = build_effective_tree(p_commit.tree, next_overrides);
	RNMountingTransaction transaction;
	transaction.runtime_generation = p_commit.runtime_generation;
	transaction.root_tag = p_commit.root_tag;
	transaction.surface_epoch = p_commit.surface_epoch;
	transaction.revision = p_commit.revision;
	if (!prepare_transaction(transaction, effective, p_constraint, r_error)) {
		destroy_detached_hosts(transaction);
		String ignored;
		HashMap<int, RNPreparedHostState> retained;
		for (const KeyValue<int, RNMountedNode> &entry : mounted_nodes) {
			retained[entry.key] = entry.value.prepared_state;
		}
		layout_tree.rebuild(committed_root, p_constraint, ignored, &retained);
		stats.rejected_transactions++;
		return false;
	}
	if (!apply_transaction(transaction, p_commit.tree, next_overrides, p_constraint, p_window_transform, r_events, r_error)) {
		stats.rejected_transactions++;
		return false;
	}
	declarative_root = p_commit.tree;
	return true;
}

bool RNMountingManager::resize(const Size2 &p_constraint, const Transform2D &p_window_transform, Vector<RNNativeEvent> &r_events, String &r_error) {
	if (committed_root.is_null()) {
		return true;
	}
	RNMountingTransaction transaction;
	transaction.runtime_generation = runtime_generation;
	transaction.root_tag = root_tag;
	transaction.surface_epoch = surface_epoch;
	transaction.revision = published_revision;
	if (!prepare_transaction(transaction, committed_root, p_constraint, r_error)) {
		return false;
	}
	return apply_transaction(transaction, declarative_root, direct_prop_overrides, p_constraint, p_window_transform, r_events, r_error);
}

bool RNMountingManager::apply_direct_props(int p_tag, const Dictionary &p_patch, const Size2 &p_constraint, const Transform2D &p_window_transform, Vector<RNNativeEvent> &r_events, String &r_error) {
	if (declarative_root.is_null() || !mounted_nodes.has(p_tag)) {
		r_error = vformat("cannot apply direct props to unmounted tag %d", p_tag);
		return false;
	}
	HashMap<int, Dictionary> next_overrides = copy_overrides(direct_prop_overrides);
	Dictionary &overrides = next_overrides[p_tag];
	const Array keys = p_patch.keys();
	for (int i = 0; i < keys.size(); ++i) {
		if (p_patch[keys[i]].get_type() == Variant::NIL) {
			overrides.erase(keys[i]);
		} else {
			overrides[keys[i]] = p_patch[keys[i]];
		}
	}
	if (overrides.is_empty()) {
		next_overrides.erase(p_tag);
	}
	Ref<RNShadowNode> effective = build_effective_tree(declarative_root, next_overrides);
	RNMountingTransaction transaction;
	transaction.runtime_generation = runtime_generation;
	transaction.root_tag = root_tag;
	transaction.surface_epoch = surface_epoch;
	transaction.revision = published_revision;
	if (!prepare_transaction(transaction, effective, p_constraint, r_error)) {
		destroy_detached_hosts(transaction);
		String ignored;
		HashMap<int, RNPreparedHostState> retained;
		for (const KeyValue<int, RNMountedNode> &entry : mounted_nodes) {
			retained[entry.key] = entry.value.prepared_state;
		}
		layout_tree.rebuild(committed_root, p_constraint, ignored, &retained);
		return false;
	}
	return apply_transaction(transaction, declarative_root, next_overrides, p_constraint, p_window_transform, r_events, r_error);
}

bool RNMountingManager::dispatch_command(int p_tag, const StringName &p_command, const Variant &p_arguments, String &r_error) {
	RNMountedNode *mounted = mounted_nodes.getptr(p_tag);
	Control *host = mounted ? Object::cast_to<Control>(ObjectDB::get_instance(mounted->object_id)) : nullptr;
	if (!mounted || !host || !mounted->descriptor) {
		r_error = vformat("cannot dispatch command to unmounted tag %d", p_tag);
		return false;
	}
	RNError error;
	if (!mounted->descriptor->dispatch_command(host, p_command, p_arguments, host_context(p_tag, published_revision), error)) {
		r_error = error.describe();
		return false;
	}
	return true;
}

bool RNMountingManager::is_current_host(int p_tag, ObjectID p_id, uint64_t p_revision) const {
	const RNMountedNode *mounted = mounted_nodes.getptr(p_tag);
	return !transaction_in_flight && mounted && mounted->object_id == p_id && mounted->last_changed_revision == p_revision && ObjectDB::get_instance(p_id);
}

void RNMountingManager::publish_transform(const Transform2D &p_window_transform) {
	if (!published_snapshot) {
		return;
	}
	auto replacement = std::make_shared<RNSurfaceSnapshot>(*published_snapshot);
	refresh_geometry(*replacement, p_window_transform);
	replacement->native_visual_revision++;
	published_snapshot = replacement;
}

void RNMountingManager::refresh_geometry(RNSurfaceSnapshot &r_snapshot, const Transform2D &p_window_transform) const {
	for (KeyValue<int, RNMountedNodeSnapshot> &entry : r_snapshot.nodes) {
		RNMountedNodeSnapshot &node = entry.value;
		node.span_rects.clear();
		node.has_visual_geometry = true;
		node.visual_transform = Transform2D(0, node.root_rect.position);
		node.viewport_rect = Rect2(Point2(), node.local_rect.size);
		node.content_size = node.local_rect.size;
		Control *host = Object::cast_to<Control>(ObjectDB::get_instance(node.object_id));
		if (host && host->is_inside_tree() && node.shadow_node->descriptor) {
			const RNHostGeometry geometry = node.shadow_node->descriptor->read_geometry(host, host_context(node.tag, r_snapshot.revision));
			const Transform2D root_screen = owner->get_screen_transform();
			node.visual_transform = !Math::is_zero_approx(root_screen.determinant()) ? root_screen.affine_inverse() * geometry.screen_transform : Transform2D(0, 0, 0, 0, 0, 0);
			node.viewport_id = geometry.viewport_id;
			node.viewport_rect = geometry.viewport;
			node.scroll_offset = geometry.scroll_offset;
			node.content_size = geometry.content_size;
			node.span_bounds = geometry.span_bounds;
			node.clips_contents = geometry.clips_contents;
			node.window_rect = geometry.transform.xform(geometry.bounds);
		}
		node.transform_invertible = !Math::is_zero_approx(node.visual_transform.determinant());
		if (node.transform_invertible) {
			node.inverse_visual_transform = node.visual_transform.affine_inverse();
		}
		if (!host || !host->is_inside_tree()) {
			node.window_rect = (p_window_transform * node.visual_transform).xform(Rect2(Point2(), node.local_rect.size));
		}
		node.paint_child_tags = node.child_tags;
		Control *container = native_parent(node.tag);
		if (container) {
			Vector<int> native_tags;
			for (int i = 0; i < container->get_child_count(); ++i) {
				const int tag = registry.get_tag(container->get_child(i)->get_instance_id());
				if (node.child_tags.has(tag)) {
					native_tags.push_back(tag);
				}
			}
			if (native_tags.size() == node.child_tags.size()) {
				node.paint_child_tags = native_tags;
			}
		}
	}
	for (const KeyValue<int, RNMountedNodeSnapshot> &entry : r_snapshot.nodes) {
		const RNMountedNodeSnapshot &paragraph = entry.value;
		for (const KeyValue<int, Vector<Rect2>> &span : paragraph.span_bounds) {
			RNMountedNodeSnapshot *node = r_snapshot.nodes.getptr(span.key);
			if (!node || span.value.is_empty()) {
				continue;
			}
			Rect2 bounds = span.value[0];
			for (const Rect2 &rect : span.value) {
				bounds = bounds.merge(rect);
			}
			node->has_visual_geometry = true;
			node->visual_transform = paragraph.visual_transform * Transform2D(0, bounds.position);
			node->transform_invertible = paragraph.transform_invertible;
			if (node->transform_invertible) {
				node->inverse_visual_transform = node->visual_transform.affine_inverse();
			}
			node->local_rect = bounds;
			node->root_rect = paragraph.visual_transform.xform(bounds);
			node->window_rect = p_window_transform.xform(node->root_rect);
			node->viewport_id = paragraph.viewport_id;
			node->viewport_rect = Rect2(Point2(), bounds.size);
			for (const Rect2 &rect : span.value) {
				node->span_rects.push_back(Rect2(rect.position - bounds.position, rect.size));
			}
		}
	}
}

void RNMountingManager::sort_paint_order(HashMap<int, RNMountedNode> &r_records) {
	HashSet<int> parents;
	for (const KeyValue<int, RNMountedNode> &entry : r_records) {
		if (entry.value.renderer_owned) {
			parents.insert(entry.value.native_parent_tag);
		}
	}
	for (int parent_tag : parents) {
		Control *parent = native_parent(parent_tag);
		if (!parent) {
			continue;
		}
		Vector<int> slots;
		Vector<int> tags;
		for (int i = 0; i < parent->get_child_count(); ++i) {
			const int tag = registry.get_tag(parent->get_child(i)->get_instance_id());
			if (r_records.has(tag)) {
				slots.push_back(i);
				tags.push_back(tag);
			}
		}
		for (int i = 1; i < tags.size(); ++i) {
			for (int j = i; j > 0; --j) {
				const int current = int(r_records[tags[j]].prepared_state.props.get("zIndex", 0));
				const int previous = int(r_records[tags[j - 1]].prepared_state.props.get("zIndex", 0));
				if (current > previous || (current == previous && r_records[tags[j]].native_index >= r_records[tags[j - 1]].native_index)) {
					break;
				}
				std::swap(tags.write[j], tags.write[j - 1]);
			}
		}
		for (int i = 0; i < tags.size(); ++i) {
			Node *desired = Object::cast_to<Node>(ObjectDB::get_instance(r_records[tags[i]].object_id));
			const int previous_index = desired->get_index();
			if (previous_index != slots[i]) {
				Node *displaced = parent->get_child(slots[i]);
				parent->move_child(desired, slots[i]);
				parent->move_child(displaced, previous_index);
			}
		}
		for (int i = 0; i < tags.size(); ++i) {
			r_records[tags[i]].native_index = i;
		}
	}
}

int RNMountingManager::tag_for_input_control(Control *p_control) const {
	for (Node *node = p_control; node; node = node->get_parent()) {
		const int tag = registry.get_tag(node->get_instance_id());
		if (tag) {
			const RNMountedNode *record = mounted_nodes.getptr(tag);
			return record && record->descriptor && record->descriptor->owns_input_control(Object::cast_to<Control>(node), p_control) ? tag : 0;
		}
	}
	return 0;
}
Control *RNMountingManager::focus_control(int p_tag) const {
	const RNMountedNode *record = mounted_nodes.getptr(p_tag);
	Control *host = host_for_tag(p_tag);
	return host && record && record->descriptor ? record->descriptor->focus_control(host) : nullptr;
}
bool RNMountingManager::owns_native_activation(int p_tag) const {
	const RNMountedNode *record = mounted_nodes.getptr(p_tag);
	return record && record->descriptor && record->descriptor->owns_native_activation(record->prepared_state);
}
