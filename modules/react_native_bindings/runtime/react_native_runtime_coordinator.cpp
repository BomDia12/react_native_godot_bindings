#include "react_native_runtime_coordinator.h"

#include "../components/rn_host_descriptor_jsi.h"
#include "../components/rn_host_descriptor_registry.h"
#include "../fabric/fabric_ui_manager.h"
#include "../fabric/native_dom.h"
#include "../native_modules/rn_blob_service.h"
#include "../native_modules/rn_http_service.h"
#include "../native_modules/rn_native_module_registry.h"
#include "../root_view/react_native_root_view.h"
#include "../singletons/hermes_runtime_singleton.h"
#include "../singletons/react_native_file_singleton.h"
#include "rn_runtime_scheduler.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/io/config_file.h"
#include "core/object/callable_mp.h"
#include "core/object/object.h"
#include "core/string/translation_server.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/text/text_server.h"

#include <algorithm>
#include <climits>
#include <unordered_map>
#include <vector>

namespace {
constexpr const char *RUN_APPLICATION_FUNCTION = "__godotRunApplication";
constexpr const char *STOP_APPLICATION_FUNCTION = "__godotStopApplication";
constexpr const char *NATIVE_DOM_GLOBAL = "__godotNativeDOM";
constexpr const char *HOST_DESCRIPTORS_GLOBAL = "__godotHostDescriptors";
constexpr const char *NATIVE_MODULES_GLOBAL = "__godotNativeModules";

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
	state->descriptor_registry = std::make_shared<RNHostDescriptorRegistry>();
	ui_manager = std::make_shared<FabricUIManager>(state);
	state->ui_manager = ui_manager;
	native_dom = std::make_shared<NativeDOM>(state);
	descriptor_jsi_registry = std::make_shared<RNHostDescriptorJSIRegistry>(state->descriptor_registry);
	native_module_registry = std::make_shared<RNNativeModuleRegistry>(state);
	scheduler = std::make_shared<RNRuntimeScheduler>();

	HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton();
	ERR_FAIL_NULL(hermes);
	hermes->install_host_object(FabricUIManager::GLOBAL_NAME, ui_manager);
	hermes->install_host_object(NATIVE_DOM_GLOBAL, native_dom);
	hermes->install_host_object(HOST_DESCRIPTORS_GLOBAL, descriptor_jsi_registry);
	hermes->install_host_object(NATIVE_MODULES_GLOBAL, native_module_registry);
	hermes->install_host_object("__godotScheduler", scheduler);

	ReactNativeFileSingleton *files = ReactNativeFileSingleton::get_singleton();
	if (files) {
		files->connect("react_native_file_changed", callable_mp(this, &ReactNativeRuntimeCoordinator::_on_react_native_file_changed));
	}
}

void ReactNativeRuntimeCoordinator::shutdown_scene() {
	state->shutting_down = true;
	disconnect_frame_signal();
	native_module_registry->begin_generation(0);
	if (state->http && ObjectDB::get_instance(state->http_id)) {
		state->http->shutdown();
		state->http.reset();
	}
	if (state->blobs) {
		state->blobs->shutdown();
		state->blobs.reset();
	}
	state->images.reset();
	clear_generation_state();
}

ReactNativeRuntimeCoordinator::~ReactNativeRuntimeCoordinator() {
	state->shutting_down = true;
	disconnect_frame_signal();
	ReactNativeFileSingleton *files = ReactNativeFileSingleton::get_singleton();
	if (files && files->is_connected("react_native_file_changed", callable_mp(this, &ReactNativeRuntimeCoordinator::_on_react_native_file_changed))) {
		files->disconnect("react_native_file_changed", callable_mp(this, &ReactNativeRuntimeCoordinator::_on_react_native_file_changed));
	}
	if (HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton()) {
		hermes->uninstall_host_object("__godotBlobCollectors");
		hermes->uninstall_host_object("__godotScheduler");
		hermes->uninstall_host_object(NATIVE_MODULES_GLOBAL);
		hermes->uninstall_host_object(NATIVE_DOM_GLOBAL);
		hermes->uninstall_host_object(HOST_DESCRIPTORS_GLOBAL);
		hermes->uninstall_host_object(FabricUIManager::GLOBAL_NAME);
	}
	native_dom.reset();
	native_module_registry.reset();
	descriptor_jsi_registry.reset();
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
	RNError settings_error;
	if (!RNServiceSettings::snapshot(state->service_settings, settings_error)) {
		state->bundle_status = RNBundleStatus::FAILED;
		state->bundle_error = settings_error.describe();
		return false;
	}
	state->font_scale = state->service_settings.font_scale;
	ProjectSettings *project = ProjectSettings::get_singleton();
	Ref<ConfigFile> direction;
	direction.instantiate();
	direction->load("user://react_native_direction.cfg");
	state->swap_rtl = direction->get_value("direction", "swap_rtl", project->get_setting("react_native/i18n/swap_rtl", true));
	state->force_rtl = direction->get_value("direction", "force_rtl", project->get_setting("react_native/i18n/force_rtl", false));
	state->allow_rtl = direction->get_value("direction", "allow_rtl", project->get_setting("react_native/i18n/allow_rtl", true));
	state->is_rtl = state->force_rtl || (state->allow_rtl && TextServerManager::get_singleton()->get_primary_interface()->is_locale_right_to_left(TranslationServer::get_singleton()->get_locale()));
	if (state->blobs) {
		state->blobs->shutdown();
	}
	if (state->http && ObjectDB::get_instance(state->http_id)) {
		state->http->shutdown();
	}
	state->http.reset();
	state->blobs = std::make_shared<RNBlobService>(state->service_settings.limit("binary/max_blob_bytes"));
	hermes->install_host_object("__godotBlobCollectors", state->blobs->collector_provider());
	RNHTTPService *http = memnew(RNHTTPService(state->service_settings));
	SceneTree *tree = SceneTree::get_singleton();
	if (!tree || !tree->get_root()) {
		memdelete(http);
		state->bundle_error = "HTTP services require a SceneTree.";
		state->bundle_status = RNBundleStatus::FAILED;
		return false;
	}
	tree->get_root()->call_deferred("add_child", http);
	state->http_id = http->get_instance_id();
	state->http = std::shared_ptr<RNHTTPService>(http, [id = state->http_id](RNHTTPService *p_service) {
		if (ObjectDB::get_instance(id) != p_service) { return; } if (p_service->get_parent()) { p_service->get_parent()->remove_child(p_service); } memdelete(p_service); });
	state->images = RNImageService::for_generation(generation);
	state->images->set_transport(rn_http_image_transport(state->http));
	scheduler->configure(state->service_settings.limit("scheduler/max_tasks_per_frame"), state->service_settings.limit("scheduler/idle_budget_ms"));
	state->bundle_generation = generation;
	native_module_registry->begin_generation(generation);
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
	if (RNJSNativeCallScope::active()) {
		pending_lifecycle.push_back([this, id] {
			auto root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(ObjectID(id)));
			if (root && root->is_inside_tree()) {
				register_root(root);
			}
		});
		return;
	}
	state->registered_roots[id] = p_root->get_application_key();
	connect_frame_signal(p_root);
	if (!find_route(p_root->get_instance_id())) {
		start_root(p_root);
	}
}

void ReactNativeRuntimeCoordinator::stop_route(RNSurfaceRoute &p_route, bool p_dispatch_cancellations) {
	p_route.status = RNSurfaceStatus::STOPPING;
	native_module_registry->close_surface(p_route.root_tag, p_route.surface_epoch);
	state->operation_queues.erase(p_route.root_tag);
	ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(p_route.root_view_id));
	if (root && p_dispatch_cancellations) {
		enqueue_events(root->_prepare_surface_stop());
	}
	enqueue_events(state->pointer_capture.clear_surface(p_route.root_tag, p_route.surface_epoch, p_route.runtime_generation));
	auto stop_application = [this, tag = p_route.root_tag] {
		if (HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton()) {
			hermes->dispatch_queued_events(ui_manager);
			Array args;
			args.push_back(tag);
			hermes->call_function(STOP_APPLICATION_FUNCTION, args);
		}
	};
	if (RNJSNativeCallScope::active()) {
		pending_lifecycle.push_back(stop_application);
	} else {
		stop_application();
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
}

void ReactNativeRuntimeCoordinator::reload_root(ReactNativeRootView *p_root) {
	ERR_FAIL_NULL(p_root);
	if (RNJSNativeCallScope::active()) {
		const ObjectID id = p_root->get_instance_id();
		pending_lifecycle.push_back([this, id] {
			if (auto root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(id))) {
				reload_root(root);
			}
		});
		return;
	}
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
		std::shared_ptr<const RNSurfaceSnapshot> old_snapshot;
		auto old = state->snapshots.find(p_snapshot->root_tag);
		if (old != state->snapshots.end()) {
			old_snapshot = old->second;
		}
		state->snapshots[p_snapshot->root_tag] = p_snapshot;
		if (ui_manager) {
			ui_manager->reconcile_surface(*p_snapshot);
		}
		Vector<RNNativeEvent> events = state->pointer_capture.reconcile_surface(*p_snapshot);
		if (old_snapshot) {
			for (RNNativeEvent &event : events) {
				const RNMountedNodeSnapshot *old_node = old_snapshot->nodes.getptr(event.tag);
				if (old_node && old_node->shadow_node.is_valid()) {
					event.retained_target = old_node->shadow_node->event_target;
				}
			}
		}
		enqueue_events(events);
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

void ReactNativeRuntimeCoordinator::reject_commit(int p_root_tag, uint64_t p_epoch, uint64_t p_revision, const String &p_error) {
	auto found = state->routes.find(p_root_tag);
	if (found == state->routes.end() || found->second.surface_epoch != p_epoch) {
		return;
	}
	found->second.error = vformat("Revision %d rejected: %s", p_revision, p_error);
	found->second.rejected_revision = p_revision;
	ERR_PRINT(vformat("React Native surface %d revision %d rejected: %s", p_root_tag, p_revision, p_error));
}

void ReactNativeRuntimeCoordinator::mark_surface_mounted(int p_root_tag, uint64_t p_epoch, uint64_t p_revision) {
	auto found = state->routes.find(p_root_tag);
	if (found == state->routes.end() || found->second.surface_epoch != p_epoch) {
		return;
	}
	found->second.mounted_revision = p_revision;
	if (found->second.rejected_revision < p_revision) {
		found->second.error = String();
	}
	if (found->second.status == RNSurfaceStatus::STARTING) {
		found->second.status = RNSurfaceStatus::ACTIVE;
	}
}

void ReactNativeRuntimeCoordinator::clear_generation_state() {
	state->operation_queues.clear();
	state->event_queue.clear();
	state->desired_nodes.clear();
	state->snapshots.clear();
	state->pointer_capture.clear();
	state->coalesced_commits = 0;
}

void ReactNativeRuntimeCoordinator::_on_react_native_file_changed(const String &p_path, const String &p_content, bool p_exists) {
	if (RNJSNativeCallScope::active()) {
		pending_lifecycle.push_back([this] { _on_react_native_file_changed(String(), String(), false); });
		return;
	}
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
	const double frame_started = scheduler->now();
	const int frame_rate = Engine::get_singleton()->get_max_fps();
	const double frame_budget = 1000.0 / (frame_rate > 0 ? frame_rate : 60);
	std::deque<std::function<void()>> lifecycle;
	lifecycle.swap(pending_lifecycle);
	for (auto &operation : lifecycle) {
		operation();
	}
	if (state->shutting_down || (state->registered_roots.empty() && !native_module_registry->has_pending_work() && !scheduler->has_pending_work() && (!state->http || !ObjectDB::get_instance(state->http_id) || !state->http->has_pending_work()) && (!state->blobs || !state->blobs->has_pending_work()))) {
		disconnect_frame_signal();
		return;
	}
	HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton();
	if (!hermes) {
		return;
	}
	if (state->blobs) {
		state->blobs->drain_releases();
	}
	if (state->http && ObjectDB::get_instance(state->http_id)) {
		state->http->process_requests();
	}
	native_module_registry->process_jobs();
	native_module_registry->process_frame(scheduler->now());
	const size_t native_delivered = hermes->dispatch_native_module_deliveries(native_module_registry);
	hermes->dispatch_queued_events(ui_manager);
	bool visual_frame = false;
	for (const auto &entry : state->registered_roots) {
		auto root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(ObjectID(entry.first)));
		visual_frame = visual_frame || (root && root->is_visible_in_tree() && root->get_size().x > 0 && root->get_size().y > 0);
	}
	hermes->dispatch_scheduler(scheduler, false, visual_frame, 0, native_delivered);

	for (auto &queue_entry : state->operation_queues) {
		std::deque<RNSurfaceOperation> &operation_queue = queue_entry.second;
		const size_t operation_count = operation_queue.size();
		size_t processed = 0;
		while (processed < operation_count && !operation_queue.empty()) {
			RNSurfaceOperation operation = operation_queue.front();
			operation_queue.pop_front();
			processed++;
			if (operation.kind == RNSurfaceOperationKind::COMMIT) {
				RNPendingCommit commit = operation.commit;
				while (processed < operation_count && !operation_queue.empty()) {
					const RNSurfaceOperation &next = operation_queue.front();
					if (next.kind != RNSurfaceOperationKind::COMMIT || next.commit.root_tag != commit.root_tag) {
						break;
					}
					commit = next.commit;
					operation_queue.pop_front();
					processed++;
					state->coalesced_commits++;
				}
				auto route = state->routes.find(commit.root_tag);
				if (route == state->routes.end() || route->second.runtime_generation != commit.runtime_generation || route->second.surface_epoch != commit.surface_epoch || commit.revision <= route->second.mounted_revision) {
					continue;
				}
				ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(route->second.root_view_id));
				if (root) {
					root->_accept_commit(commit);
				}
				continue;
			}

			const RNImperativeRequest &request = operation.imperative;
			auto route = state->routes.find(request.root_tag);
			if (route == state->routes.end() || route->second.runtime_generation != request.runtime_generation || route->second.surface_epoch != request.surface_epoch) {
				continue;
			}
			if (route->second.mounted_revision < request.required_revision) {
				if (route->second.rejected_revision == request.required_revision) {
					route->second.error = vformat("Imperative work for surface %d tag %d requires rejected revision %d; published revision is %d.", request.root_tag, request.tag, request.required_revision, route->second.mounted_revision);
					ERR_PRINT(route->second.error);
				} else {
					operation_queue.push_front(operation);
					break;
				}
				continue;
			}
			ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(route->second.root_view_id));
			if (root) {
				root->_apply_imperative(request);
			}
		}
	}
	hermes->dispatch_scheduler(scheduler, true, false, std::max(0.0, frame_budget - (scheduler->now() - frame_started)));
}
