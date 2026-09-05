#include "react_native_runtime_coordinator.h"

#include "../fabric/fabric_ui_manager.h"
#include "../fabric/native_dom.h"
#include "../root_view/react_native_root_view.h"
#include "../singletons/hermes_runtime_singleton.h"
#include "../singletons/react_native_file_singleton.h"

#include "core/error/error_macros.h"
#include "core/object/callable_mp.h"
#include "core/object/object.h"
#include "scene/main/scene_tree.h"

#include <climits>
#include <unordered_map>
#include <vector>

namespace {
constexpr const char *RUN_APPLICATION_FUNCTION = "__godotRunApplication";
constexpr const char *STOP_APPLICATION_FUNCTION = "__godotStopApplication";
constexpr const char *FLUSH_TIMERS_FUNCTION = "__godotFlushTimers";
constexpr const char *NATIVE_DOM_GLOBAL = "__godotNativeDOM";

bool is_pointer_event(const String &p_name) {
	return p_name.begins_with("topPointer");
}

int pointer_id_of(const Dictionary &p_payload) {
	return int(p_payload.get("pointerId", 0));
}
} // namespace

ReactNativeRuntimeCoordinator *ReactNativeRuntimeCoordinator::singleton = nullptr;

void RNPointerCaptureProcessor::observe(const RNNativeEvent &p_event) {
	if (!is_pointer_event(p_event.name)) {
		return;
	}
	const int pointer_id = pointer_id_of(p_event.payload);
	if (pointer_id == 0) {
		return;
	}
	const PointerKey key{ p_event.root_tag, pointer_id };
	if (p_event.name == "topPointerDown") {
		PointerState &pointer = pointers[key];
		pointer.surface_epoch = p_event.surface_epoch;
		pointer.active_target = p_event.tag;
		pointer.sample = p_event.payload.duplicate(true);
		return;
	}
	auto found = pointers.find(key);
	if (found == pointers.end() || found->second.surface_epoch != p_event.surface_epoch) {
		return;
	}
	found->second.sample = p_event.payload.duplicate(true);
	if (p_event.name == "topPointerUp" || p_event.name == "topPointerCancel") {
		found->second.pending_target = 0;
	}
}

Vector<RNNativeEvent> RNPointerCaptureProcessor::apply_pending(const RNNativeEvent &p_event) {
	Vector<RNNativeEvent> result;
	if (!is_pointer_event(p_event.name) || p_event.name == "topPointerDown") {
		return result;
	}
	const int pointer_id = pointer_id_of(p_event.payload);
	auto found = pointers.find({ p_event.root_tag, pointer_id });
	if (found == pointers.end() || found->second.surface_epoch != p_event.surface_epoch) {
		return result;
	}
	PointerState &pointer = found->second;
	if (pointer.capture_target == pointer.pending_target) {
		return result;
	}
	if (pointer.capture_target != 0) {
		RNNativeEvent lost = p_event;
		lost.tag = pointer.capture_target;
		lost.name = "topLostPointerCapture";
		lost.priority = 1;
		lost.payload = pointer.sample.duplicate(true);
		result.push_back(lost);
	}
	pointer.capture_target = pointer.pending_target;
	if (pointer.capture_target != 0) {
		RNNativeEvent got = p_event;
		got.tag = pointer.capture_target;
		got.name = "topGotPointerCapture";
		got.priority = 1;
		got.payload = pointer.sample.duplicate(true);
		result.push_back(got);
	}
	return result;
}

bool RNPointerCaptureProcessor::has_capture(int p_root_tag, uint64_t p_epoch, int p_tag, int p_pointer_id) const {
	auto found = pointers.find({ p_root_tag, p_pointer_id });
	return found != pointers.end() && found->second.surface_epoch == p_epoch && found->second.pending_target == p_tag;
}

void RNPointerCaptureProcessor::set_capture(int p_root_tag, uint64_t p_epoch, int p_tag, int p_pointer_id) {
	auto found = pointers.find({ p_root_tag, p_pointer_id });
	if (found != pointers.end() && found->second.surface_epoch == p_epoch && found->second.active_target != 0) {
		found->second.pending_target = p_tag;
	}
}

void RNPointerCaptureProcessor::release_capture(int p_root_tag, uint64_t p_epoch, int p_tag, int p_pointer_id) {
	auto found = pointers.find({ p_root_tag, p_pointer_id });
	if (found != pointers.end() && found->second.surface_epoch == p_epoch && found->second.pending_target == p_tag) {
		found->second.pending_target = 0;
	}
}

int RNPointerCaptureProcessor::captured_target(int p_root_tag, uint64_t p_epoch, int p_pointer_id) const {
	auto found = pointers.find({ p_root_tag, p_pointer_id });
	return found != pointers.end() && found->second.surface_epoch == p_epoch ? found->second.capture_target : 0;
}

void RNPointerCaptureProcessor::finish(int p_root_tag, uint64_t p_epoch, int p_pointer_id) {
	auto found = pointers.find({ p_root_tag, p_pointer_id });
	if (found != pointers.end() && found->second.surface_epoch == p_epoch) {
		pointers.erase(found);
	}
}

Vector<RNNativeEvent> RNPointerCaptureProcessor::reconcile_surface(const RNSurfaceSnapshot &p_snapshot) {
	Vector<RNNativeEvent> result;
	for (auto it = pointers.begin(); it != pointers.end();) {
		PointerState &pointer = it->second;
		if (it->first.root_tag != p_snapshot.root_tag || pointer.surface_epoch != p_snapshot.surface_epoch) {
			++it;
			continue;
		}
		const int target = pointer.capture_target != 0 ? pointer.capture_target : pointer.pending_target;
		if (target == 0 || p_snapshot.nodes.has(target)) {
			++it;
			continue;
		}
		RNNativeEvent event;
		event.root_tag = p_snapshot.root_tag;
		event.tag = target;
		event.name = "topLostPointerCapture";
		event.priority = 1;
		event.generation = p_snapshot.runtime_generation;
		event.surface_epoch = p_snapshot.surface_epoch;
		event.payload = pointer.sample.duplicate(true);
		result.push_back(event);
		it = pointers.erase(it);
	}
	return result;
}

Vector<RNNativeEvent> RNPointerCaptureProcessor::clear_surface(int p_root_tag, uint64_t p_epoch, uint64_t p_generation) {
	Vector<RNNativeEvent> result;
	for (auto it = pointers.begin(); it != pointers.end();) {
		if (it->first.root_tag != p_root_tag || it->second.surface_epoch != p_epoch) {
			++it;
			continue;
		}
		if (it->second.capture_target != 0 || it->second.pending_target != 0) {
			RNNativeEvent event;
			event.root_tag = p_root_tag;
			event.tag = it->second.capture_target != 0 ? it->second.capture_target : it->second.pending_target;
			event.name = "topLostPointerCapture";
			event.priority = 1;
			event.generation = p_generation;
			event.surface_epoch = p_epoch;
			event.payload = it->second.sample.duplicate(true);
			result.push_back(event);
		}
		it = pointers.erase(it);
	}
	return result;
}

void RNPointerCaptureProcessor::clear() {
	pointers.clear();
}

ReactNativeRuntimeCoordinator::ReactNativeRuntimeCoordinator() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "ReactNativeRuntimeCoordinator is a singleton.");
	singleton = this;
	state = std::make_shared<RNRuntimeCoordinatorState>();
	ui_manager = std::make_shared<FabricUIManager>(state);
	state->ui_manager = ui_manager;
	native_dom = std::make_shared<NativeDOM>(state);

	HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton();
	ERR_FAIL_NULL(hermes);
	hermes->install_host_object(FabricUIManager::GLOBAL_NAME, ui_manager);
	hermes->install_host_object(NATIVE_DOM_GLOBAL, native_dom);

	ReactNativeFileSingleton *files = ReactNativeFileSingleton::get_singleton();
	if (files) {
		files->connect("react_native_file_changed", callable_mp(this, &ReactNativeRuntimeCoordinator::_on_react_native_file_changed));
	}
}

ReactNativeRuntimeCoordinator::~ReactNativeRuntimeCoordinator() {
	state->shutting_down = true;
	disconnect_frame_signal();
	ReactNativeFileSingleton *files = ReactNativeFileSingleton::get_singleton();
	if (files && files->is_connected("react_native_file_changed", callable_mp(this, &ReactNativeRuntimeCoordinator::_on_react_native_file_changed))) {
		files->disconnect("react_native_file_changed", callable_mp(this, &ReactNativeRuntimeCoordinator::_on_react_native_file_changed));
	}
	if (HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton()) {
		hermes->uninstall_host_object(NATIVE_DOM_GLOBAL);
		hermes->uninstall_host_object(FabricUIManager::GLOBAL_NAME);
	}
	native_dom.reset();
	ui_manager.reset();
	state.reset();
	if (singleton == this) {
		singleton = nullptr;
	}
}

void ReactNativeRuntimeCoordinator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_process_frame"), &ReactNativeRuntimeCoordinator::_process_frame);
}

ReactNativeRuntimeCoordinator *ReactNativeRuntimeCoordinator::get_singleton() {
	return singleton;
}

int ReactNativeRuntimeCoordinator::allocate_root_tag() {
	if (state->next_root_tag > INT_MAX) {
		ERR_PRINT("ReactNativeRuntimeCoordinator: root tag sequence exhausted.");
		return 0;
	}
	const int tag = int(state->next_root_tag);
	state->next_root_tag += 10;
	return tag;
}

RNSurfaceRoute *ReactNativeRuntimeCoordinator::find_route(ObjectID p_root_id) {
	auto found_tag = state->root_tags_by_object_id.find(uint64_t(p_root_id));
	if (found_tag == state->root_tags_by_object_id.end()) {
		return nullptr;
	}
	auto found = state->routes.find(found_tag->second);
	return found == state->routes.end() ? nullptr : &found->second;
}

void ReactNativeRuntimeCoordinator::connect_frame_signal(ReactNativeRootView *p_root) {
	if (frame_connected || !p_root || !p_root->get_tree()) {
		return;
	}
	SceneTree *tree = p_root->get_tree();
	connected_tree_id = tree->get_instance_id();
	tree->connect("process_frame", callable_mp(this, &ReactNativeRuntimeCoordinator::_process_frame));
	frame_connected = true;
}

void ReactNativeRuntimeCoordinator::disconnect_frame_signal() {
	if (!frame_connected) {
		return;
	}
	SceneTree *tree = Object::cast_to<SceneTree>(ObjectDB::get_instance(connected_tree_id));
	if (tree && tree->is_connected("process_frame", callable_mp(this, &ReactNativeRuntimeCoordinator::_process_frame))) {
		tree->disconnect("process_frame", callable_mp(this, &ReactNativeRuntimeCoordinator::_process_frame));
	}
	connected_tree_id = ObjectID();
	frame_connected = false;
}

bool ReactNativeRuntimeCoordinator::ensure_bundle() {
	HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton();
	ReactNativeFileSingleton *files = ReactNativeFileSingleton::get_singleton();
	if (!hermes) {
		return false;
	}
	const uint64_t generation = hermes->get_runtime_generation();
	if (state->bundle_generation == generation && state->bundle_status == RNBundleStatus::READY) {
		return true;
	}
	if (state->bundle_generation == generation && state->bundle_status == RNBundleStatus::FAILED) {
		return false;
	}
	state->bundle_generation = generation;
	state->bundle_status = RNBundleStatus::EVALUATING;
	state->bundle_error = String();
	if (!files || !files->has_file() || files->get_file_content().is_empty()) {
		state->bundle_status = RNBundleStatus::FAILED;
		state->bundle_error = "React Native bundle is missing or empty.";
		return false;
	}
	hermes->evaluate(files->get_file_content(), "godot://bundle.js");
	state->bundle_error = hermes->get_last_error();
	state->bundle_status = state->bundle_error.is_empty() ? RNBundleStatus::READY : RNBundleStatus::FAILED;
	return state->bundle_status == RNBundleStatus::READY;
}

void ReactNativeRuntimeCoordinator::start_root(ReactNativeRootView *p_root) {
	ERR_FAIL_NULL(p_root);
	const uint64_t id = uint64_t(p_root->get_instance_id());
	const String key = p_root->get_application_key();
	const int root_tag = allocate_root_tag();
	if (root_tag == 0) {
		return;
	}
	HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton();
	ERR_FAIL_NULL(hermes);

	RNSurfaceRoute route;
	route.root_tag = root_tag;
	route.root_view_id = p_root->get_instance_id();
	route.application_key = key;
	route.runtime_generation = hermes->get_runtime_generation();
	route.surface_epoch = state->next_surface_epoch++;
	route.status = RNSurfaceStatus::REGISTERED;
	state->routes[root_tag] = route;
	state->root_tags_by_object_id[id] = root_tag;
	ui_manager->register_surface(route);
	p_root->_attach_surface(route);

	if (!ensure_bundle()) {
		fail_surface(root_tag, route.surface_epoch, state->bundle_error);
		return;
	}
	state->routes[root_tag].status = RNSurfaceStatus::STARTING;
	Array args;
	args.push_back(key);
	args.push_back(root_tag);
	hermes->call_function(RUN_APPLICATION_FUNCTION, args);
	const String error = hermes->get_last_error();
	if (!error.is_empty()) {
		fail_surface(root_tag, route.surface_epoch, error);
	}
}

void ReactNativeRuntimeCoordinator::register_root(ReactNativeRootView *p_root) {
	ERR_FAIL_NULL(p_root);
	const uint64_t id = uint64_t(p_root->get_instance_id());
	state->registered_roots[id] = p_root->get_application_key();
	connect_frame_signal(p_root);
	if (!find_route(p_root->get_instance_id())) {
		start_root(p_root);
	}
}

void ReactNativeRuntimeCoordinator::stop_route(RNSurfaceRoute &p_route, bool p_dispatch_cancellations) {
	p_route.status = RNSurfaceStatus::STOPPING;
	ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(p_route.root_view_id));
	if (root && p_dispatch_cancellations) {
		enqueue_events(root->_prepare_surface_stop());
	}
	enqueue_events(state->pointer_capture.clear_surface(p_route.root_tag, p_route.surface_epoch, p_route.runtime_generation));
	if (HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton()) {
		hermes->dispatch_queued_events(ui_manager);
		Array args;
		args.push_back(p_route.root_tag);
		hermes->call_function(STOP_APPLICATION_FUNCTION, args);
	}
	ui_manager->remove_surface(p_route.root_tag, p_route.surface_epoch);
	state->snapshots.erase(p_route.root_tag);
	state->root_tags_by_object_id.erase(uint64_t(p_route.root_view_id));
	if (root) {
		root->_detach_surface(p_route.root_tag, p_route.surface_epoch);
	}
	p_route.status = RNSurfaceStatus::DETACHED;
}

void ReactNativeRuntimeCoordinator::unregister_root(ReactNativeRootView *p_root) {
	ERR_FAIL_NULL(p_root);
	const uint64_t id = uint64_t(p_root->get_instance_id());
	if (RNSurfaceRoute *route = find_route(p_root->get_instance_id())) {
		const int tag = route->root_tag;
		stop_route(*route, true);
		state->routes.erase(tag);
	}
	state->registered_roots.erase(id);
	if (state->registered_roots.empty()) {
		disconnect_frame_signal();
	}
}

void ReactNativeRuntimeCoordinator::reload_root(ReactNativeRootView *p_root) {
	ERR_FAIL_NULL(p_root);
	if (RNSurfaceRoute *route = find_route(p_root->get_instance_id())) {
		const int tag = route->root_tag;
		stop_route(*route, true);
		state->routes.erase(tag);
	}
	if (p_root->is_inside_tree()) {
		start_root(p_root);
	}
}

void ReactNativeRuntimeCoordinator::application_key_changed(ReactNativeRootView *p_root) {
	ERR_FAIL_NULL(p_root);
	state->registered_roots[uint64_t(p_root->get_instance_id())] = p_root->get_application_key();
	reload_root(p_root);
}

void ReactNativeRuntimeCoordinator::enqueue_events(const Vector<RNNativeEvent> &p_events) {
	for (const RNNativeEvent &event : p_events) {
		if (event.name == "topPointerDown") {
			state->pointer_capture.observe(event);
		}
		state->event_queue.push_back(event);
	}
}

void ReactNativeRuntimeCoordinator::publish_snapshot(const std::shared_ptr<const RNSurfaceSnapshot> &p_snapshot) {
	if (p_snapshot) {
		state->snapshots[p_snapshot->root_tag] = p_snapshot;
		enqueue_events(state->pointer_capture.reconcile_surface(*p_snapshot));
	}
}

std::shared_ptr<const RNSurfaceSnapshot> ReactNativeRuntimeCoordinator::get_snapshot(int p_root_tag) const {
	auto found = state->snapshots.find(p_root_tag);
	return found == state->snapshots.end() ? nullptr : found->second;
}

void ReactNativeRuntimeCoordinator::fail_surface(int p_root_tag, uint64_t p_epoch, const String &p_error) {
	auto found = state->routes.find(p_root_tag);
	if (found == state->routes.end() || found->second.surface_epoch != p_epoch) {
		return;
	}
	found->second.status = RNSurfaceStatus::FAILED;
	found->second.error = p_error;
}

void ReactNativeRuntimeCoordinator::mark_surface_mounted(int p_root_tag, uint64_t p_epoch, uint64_t p_revision) {
	auto found = state->routes.find(p_root_tag);
	if (found == state->routes.end() || found->second.surface_epoch != p_epoch) {
		return;
	}
	found->second.mounted_revision = p_revision;
	if (found->second.status == RNSurfaceStatus::STARTING) {
		found->second.status = RNSurfaceStatus::ACTIVE;
	}
}

void ReactNativeRuntimeCoordinator::clear_generation_state() {
	state->commit_queue.clear();
	state->imperative_queue.clear();
	state->event_queue.clear();
	state->desired_nodes.clear();
	state->snapshots.clear();
	state->pointer_capture.clear();
}

void ReactNativeRuntimeCoordinator::_on_react_native_file_changed(const String &p_path, const String &p_content, bool p_exists) {
	(void)p_path;
	(void)p_content;
	(void)p_exists;
	std::vector<uint64_t> roots;
	for (const auto &entry : state->registered_roots) {
		roots.push_back(entry.first);
	}
	std::vector<int> tags;
	for (const auto &entry : state->routes) {
		tags.push_back(entry.first);
	}
	for (int tag : tags) {
		auto found = state->routes.find(tag);
		if (found != state->routes.end()) {
			stop_route(found->second, true);
		}
	}
	state->routes.clear();
	clear_generation_state();
	if (HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton()) {
		hermes->reset();
	}
	state->bundle_status = RNBundleStatus::UNEVALUATED;
	state->bundle_generation = 0;
	state->bundle_error = String();
	for (uint64_t id : roots) {
		ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(ObjectID(id)));
		if (root && root->is_inside_tree()) {
			start_root(root);
		}
	}
}

void ReactNativeRuntimeCoordinator::_process_frame() {
	if (state->shutting_down || state->registered_roots.empty()) {
		return;
	}
	HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton();
	if (!hermes) {
		return;
	}
	if (state->bundle_status == RNBundleStatus::READY && hermes->get_global(FLUSH_TIMERS_FUNCTION).get_type() != Variant::NIL) {
		hermes->call_function(FLUSH_TIMERS_FUNCTION);
	}
	hermes->dispatch_queued_events(ui_manager);

	std::unordered_map<int, RNPendingCommit> newest;
	while (!state->commit_queue.empty()) {
		RNPendingCommit commit = state->commit_queue.front();
		state->commit_queue.pop_front();
		auto route = state->routes.find(commit.root_tag);
		if (route != state->routes.end() && route->second.runtime_generation == commit.runtime_generation && route->second.surface_epoch == commit.surface_epoch) {
			newest[commit.root_tag] = commit;
		}
	}
	for (const auto &entry : newest) {
		const RNPendingCommit &commit = entry.second;
		auto route = state->routes.find(commit.root_tag);
		if (route == state->routes.end()) {
			continue;
		}
		ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(route->second.root_view_id));
		if (root) {
			root->_accept_commit(commit);
		}
	}

	const size_t request_count = state->imperative_queue.size();
	for (size_t i = 0; i < request_count; ++i) {
		RNImperativeRequest request = state->imperative_queue.front();
		state->imperative_queue.pop_front();
		auto route = state->routes.find(request.root_tag);
		if (route == state->routes.end() || route->second.runtime_generation != request.runtime_generation || route->second.surface_epoch != request.surface_epoch) {
			continue;
		}
		if (route->second.mounted_revision < request.required_revision) {
			state->imperative_queue.push_back(request);
			continue;
		}
		ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(route->second.root_view_id));
		if (root) {
			root->_apply_imperative(request);
		}
	}
}
