#include "react_native_host.h"

#include "../components/rn_theme_metrics.h"
#include "../root_view/react_native_root_view.h"
#include "../singletons/react_native_file_singleton.h"
#include "rn_host_instance.h"

#include "core/input/input.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

#include <mutex>

using namespace facebook::react;

void RNHostLifecycle::_notification(int p_what) {
	ReactNativeHost *host = ReactNativeHost::get_singleton();
	if (!host) {
		return;
	}
	if (p_what == NOTIFICATION_APPLICATION_PAUSED || p_what == NOTIFICATION_APPLICATION_RESUMED) {
		host->set_application_paused(p_what == NOTIFICATION_APPLICATION_PAUSED);
	} else if (p_what == NOTIFICATION_WM_GO_BACK_REQUEST && host->handles_back_press()) {
		host->back_pressed(true);
	}
}

void RNEvaluation::_bind_methods() {
	ADD_SIGNAL(MethodInfo("completed", PropertyInfo(Variant::NIL, "result", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
}

ReactNativeHost *ReactNativeHost::singleton = nullptr;

ReactNativeHost::ReactNativeHost() {
	singleton = this;
	theme = std::make_shared<RNThemeMetricsStore>();
	if (ReactNativeFileSingleton *files = ReactNativeFileSingleton::get_singleton()) {
		files->connect("react_native_file_changed", callable_mp(this, &ReactNativeHost::_bundle_changed));
	}
}

ReactNativeHost::~ReactNativeHost() {
	shutdown();
	if (ReactNativeFileSingleton *files = ReactNativeFileSingleton::get_singleton()) {
		files->disconnect("react_native_file_changed", callable_mp(this, &ReactNativeHost::_bundle_changed));
	}
	singleton = nullptr;
}

void ReactNativeHost::_bind_methods() {
	ClassDB::bind_method(D_METHOD("evaluate", "code"), &ReactNativeHost::evaluate);
	ClassDB::bind_method(D_METHOD("get_generation"), &ReactNativeHost::get_generation);
	ClassDB::bind_method(D_METHOD("get_last_error"), &ReactNativeHost::get_last_error);
	ClassDB::bind_method(D_METHOD("get_service_stats"), &ReactNativeHost::get_service_stats);
	ClassDB::bind_method(D_METHOD("reload"), &ReactNativeHost::reload);
	ClassDB::bind_method(D_METHOD("open_debugger"), &ReactNativeHost::open_debugger);
	ClassDB::bind_method(D_METHOD("post_message", "name", "payload"), &ReactNativeHost::post_message, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("register_module", "name", "target", "binding"), &ReactNativeHost::register_module);
	ClassDB::bind_method(D_METHOD("unregister_module", "name"), &ReactNativeHost::unregister_module);
	ADD_SIGNAL(MethodInfo("message_received", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::NIL, "payload", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
}

void ReactNativeHost::_record_error(uint64_t p_generation, const String &p_error) {
	std::lock_guard lock(error_mutex);
	if (p_generation == error_generation) {
		last_error = p_error;
	}
}

String ReactNativeHost::get_last_error() const {
	std::lock_guard lock(error_mutex);
	return last_error;
}

Dictionary ReactNativeHost::get_service_stats() const {
	return instance ? instance->stats() : Dictionary();
}

bool ReactNativeHost::_start() {
	if (instance) {
		return true;
	}
	SceneTree *tree = SceneTree::get_singleton();
	ERR_FAIL_COND_V_MSG(!tree || !tree->get_root(), false, "React Native needs a SceneTree.");
	RNError error;
	if (!RNServiceSettings::snapshot(settings, error)) {
		_record_error(generation, error.describe());
		ERR_PRINT(error.describe());
		return false;
	}
	ReactNativeFileSingleton *files = ReactNativeFileSingleton::get_singleton();
	const String bundle = files ? files->get_monitored_file() : String();
	if (!settings.dev_server.enabled && (!files || !files->has_file())) {
		_record_error(generation, vformat("React Native bundle '%s' is missing.", bundle));
		ERR_PRINT(get_last_error());
		return false;
	}
	if (!frame_connected) {
		tree->connect("process_frame", callable_mp(this, &ReactNativeHost::_process_frame));
		frame_connected = true;
	}
	if (!ObjectDB::get_instance(lifecycle_node)) {
		RNHostLifecycle *lifecycle = memnew(RNHostLifecycle);
		lifecycle_node = lifecycle->get_instance_id();
		tree->get_root()->call_deferred("add_child", lifecycle, false, Node::INTERNAL_MODE_BACK);
	}
	if (!roots.is_empty()) {
		if (Control *theme_owner = Object::cast_to<Control>(ObjectDB::get_instance(roots[0]))) {
			theme->publish(RNThemeMetrics::capture(theme_owner, ++theme_revision));
		}
	}

	++generation;
	{
		std::lock_guard lock(error_mutex);
		error_generation = generation;
		last_error = String();
	}
	RNHostInstance::Setup setup;
	setup.settings = settings;
	setup.theme = theme;
	setup.generation = generation;
	setup.find_root = [this](SurfaceId p_surface) { return find_root(p_surface); };
	setup.find_module = [this](const String &p_name) { return find_module(p_name); };
	setup.on_error = [this, current = generation](const String &p_error) { _record_error(current, p_error); };
	setup.reload = [this] { reload(); };
	instance = std::make_shared<RNHostInstance>(setup);
	if (!instance->load_script(bundle)) {
		_record_error(generation, vformat("React Native bundle '%s' could not be loaded.", bundle));
		ERR_PRINT(get_last_error());
	}
	_update_suspension();
	return true;
}

// The retired instance stops reaching Godot here, and its runtime finishes its
// own teardown on another thread while the next instance already runs.
void ReactNativeHost::_retire() {
	if (!instance) {
		return;
	}
	std::vector<SurfaceId> stopped;
	for (const ObjectID &id : roots) {
		if (ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(id)); root && root->get_root_tag() != 0) {
			stopped.push_back(root->get_root_tag());
			_stop_surface(root, false);
		}
	}
	for (const KeyValue<uint64_t, Ref<RNEvaluation>> &entry : evaluations) {
		callable_mp(this, &ReactNativeHost::_finish_evaluation).call_deferred(entry.key, Variant());
	}
	instance->retire(std::move(stopped));
	retiring.push_back(instance);
	instance.reset();
}

void ReactNativeHost::_finish_retired() {
	for (int i = retiring.size() - 1; i >= 0; --i) {
		if (retiring[i]->is_finished()) {
			retiring[i]->finish();
			retiring.remove_at(i);
		}
	}
}

void ReactNativeHost::_start_surface(ReactNativeRootView *p_root) {
	const SurfaceId surface = next_surface_id;
	next_surface_id += 10;
	surfaces[surface] = p_root->get_instance_id();
	_publish_theme(p_root, surface);
	instance->start_surface(p_root, surface);
}

// Each revision is new, so text measured with an earlier theme is measured again.
void ReactNativeHost::_publish_theme(ReactNativeRootView *p_root, SurfaceId p_surface) {
	theme->publish(p_surface, RNThemeMetrics::capture(p_root, ++theme_revision));
}

void ReactNativeHost::_stop_surface(ReactNativeRootView *p_root, bool p_stop_react) {
	const SurfaceId surface = p_root->get_root_tag();
	if (surface == 0) {
		return;
	}
	if (instance) {
		instance->stop_surface(p_root, p_stop_react);
	} else {
		p_root->_detach_surface();
	}
	surfaces.erase(surface);
	theme->forget(surface);
}

bool ReactNativeHost::_defer(std::function<void()> p_task) {
	if (!processing_frame) {
		return false;
	}
	deferred.push_back(std::move(p_task));
	return true;
}

void ReactNativeHost::register_root(ReactNativeRootView *p_root) {
	const ObjectID id = p_root->get_instance_id();
	if (_defer([this, id] {
			if (ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(id)); root && root->is_inside_tree()) {
				register_root(root);
			}
		})) {
		return;
	}
	if (!roots.has(id)) {
		roots.push_back(id);
	}
	if (p_root->get_root_tag() == 0 && _start()) {
		_start_surface(p_root);
	}
	_update_suspension();
}

void ReactNativeHost::unregister_root(ReactNativeRootView *p_root) {
	_stop_surface(p_root);
	roots.erase(p_root->get_instance_id());
	_update_suspension();
}

void ReactNativeHost::restart_root(ReactNativeRootView *p_root) {
	const ObjectID id = p_root->get_instance_id();
	if (_defer([this, id] {
			if (ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(id)); root && root->is_inside_tree()) {
				restart_root(root);
			}
		})) {
		return;
	}
	_stop_surface(p_root);
	if (_start()) {
		_start_surface(p_root);
	}
}

void ReactNativeHost::update_root_layout(ReactNativeRootView *p_root) {
	if (!instance || p_root->get_root_tag() == 0) {
		return;
	}
	if (!roots.is_empty() && roots[0] == p_root->get_instance_id()) {
		theme->publish(RNThemeMetrics::capture(p_root, ++theme_revision));
	}
	_publish_theme(p_root, p_root->get_root_tag());
	instance->update_layout(p_root);
}

void ReactNativeHost::scene_binding_changed(SurfaceId p_surface) {
	if (instance) {
		instance->scene_binding_changed(p_surface);
	}
}

void ReactNativeHost::root_state_changed(ReactNativeRootView *p_root) {
	if (instance && p_root->get_root_tag() != 0) {
		instance->root_state_changed(p_root->get_root_tag(), p_root->get_state());
	}
	_update_suspension();
}

// JavaScript stops while no root can process, as paused nodes do; an empty
// scene keeps it running, so evaluate() and starting roots still work.
void ReactNativeHost::_update_suspension() {
	if (!instance) {
		return;
	}
	bool processing = roots.is_empty();
	for (const ObjectID &id : roots) {
		const ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(id));
		processing = processing || (root && root->is_inside_tree() && root->can_process());
	}
	instance->set_suspended(!processing);
}

// Modules belong to the host rather than a runtime, so they survive reloads.
Dictionary ReactNativeHost::register_module(const String &p_name, Object *p_target, const Ref<RNSceneBinding> &p_binding) {
	RNError error;
	auto attachment = std::make_shared<RNSceneAttachment>();
	if (p_name.is_empty()) {
		error = RNError::make(RNErrorCode::VALIDATION, "module name is required", "registerModule", "name");
	} else if (p_binding.is_null()) {
		error = RNError::make(RNErrorCode::VALIDATION, "binding resource is required", "registerModule", "binding");
	} else {
		p_binding->compile(p_target, *attachment, error);
	}
	if (error.is_set()) {
		return error.to_dictionary();
	}
	attachment->identity = next_module_identity++;
	modules[p_name] = attachment;
	// A node outside the tree is no module target, so readiness follows the node.
	if (Node *node = Object::cast_to<Node>(p_target)) {
		const Callable moved = callable_mp(this, &ReactNativeHost::_module_node_moved).bind(p_name, attachment->identity);
		node->connect(SNAME("tree_entered"), moved);
		node->connect(SNAME("tree_exited"), moved);
	}
	if (instance) {
		instance->module_changed(p_name);
	}
	return Dictionary();
}

void ReactNativeHost::_module_node_moved(const String &p_name, uint64_t p_identity) {
	const std::shared_ptr<const RNSceneAttachment> *module = modules.getptr(p_name);
	if (module && (*module)->identity == p_identity && instance) {
		instance->module_changed(p_name);
	}
}

void ReactNativeHost::unregister_module(const String &p_name) {
	if (modules.erase(p_name) && instance) {
		instance->module_changed(p_name);
	}
}

std::shared_ptr<const RNSceneAttachment> ReactNativeHost::find_module(const String &p_name) const {
	const std::shared_ptr<const RNSceneAttachment> *module = modules.getptr(p_name);
	return module ? *module : nullptr;
}

void ReactNativeHost::post_message(const String &p_name, const Variant &p_payload) {
	send_message(std::nullopt, p_name, p_payload);
}

void ReactNativeHost::send_message(std::optional<int> p_root_tag, const String &p_name, const Variant &p_payload) {
	if (!instance) {
		WARN_PRINT(vformat("The message '%s' was dropped because React Native is not running.", p_name));
		return;
	}
	instance->send_message(p_root_tag, p_name, p_payload);
}

bool ReactNativeHost::handles_back_press() const {
	return instance && instance->handles_back_press();
}

void ReactNativeHost::back_pressed(bool p_go_back_request) {
	if (instance) {
		back_press_quits = p_go_back_request && project_quits_on_go_back;
		instance->back_pressed();
	}
}

// No BackHandler listener handled the last back press, so Godot gets what it
// would have had without them: Android's back quits when the project says
// so, and ui_cancel reaches the game.
void ReactNativeHost::invoke_default_back_press() {
	if (back_press_quits) {
		SceneTree::get_singleton()->quit();
		return;
	}
	default_back_press.instantiate();
	default_back_press->set_action(SNAME("ui_cancel"));
	default_back_press->set_pressed(true);
	Input::get_singleton()->parse_input_event(default_back_press);
	Ref<InputEventAction> release = default_back_press->duplicate();
	release->set_pressed(false);
	Input::get_singleton()->parse_input_event(release);
}

// SceneTree quits on Android's back button by itself, before JavaScript can
// handle it, so it doesn't while BackHandler has listeners.
void ReactNativeHost::_sync_back_press() {
	if (handles_back_press() == takes_back_press) {
		return;
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (!tree) {
		return;
	}
	takes_back_press = !takes_back_press;
	if (takes_back_press) {
		project_quits_on_go_back = tree->is_quit_on_go_back();
		tree->set_quit_on_go_back(false);
	} else {
		tree->set_quit_on_go_back(project_quits_on_go_back);
	}
}

ReactNativeRootView *ReactNativeHost::find_root(SurfaceId p_surface) const {
	const ObjectID *id = surfaces.getptr(p_surface);
	return id ? Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(*id)) : nullptr;
}

void ReactNativeHost::reload() {
	if (_defer([this] { reload(); })) {
		return;
	}
	_retire();
	if (!_start()) {
		return;
	}
	for (const ObjectID &id : roots) {
		if (ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(id))) {
			_start_surface(root);
		}
	}
}

void ReactNativeHost::open_debugger() {
	if (instance) {
		instance->open_debugger();
	}
}

void ReactNativeHost::_bundle_changed(const String &, const String &, bool) {
	if (instance || !roots.is_empty()) {
		reload();
	}
}

void ReactNativeHost::set_application_paused(bool p_paused) {
	if (instance) {
		instance->set_paused(p_paused);
	}
}

void ReactNativeHost::_process_frame() {
	std::deque<std::function<void()>> pending;
	pending.swap(deferred);
	for (const std::function<void()> &task : pending) {
		task();
	}
	processing_frame = true;
	if (instance) {
		instance->process_frame();
	}
	_finish_retired();
	_sync_back_press();
	for (int i = scene_loads.size() - 1; i >= 0; --i) {
		if (ResourceLoader::load_threaded_get_status(scene_loads[i]) != ResourceLoader::THREAD_LOAD_IN_PROGRESS) {
			ResourceLoader::load_threaded_get(scene_loads[i]);
			scene_loads.remove_at(i);
		}
	}
	processing_frame = false;
}

Signal ReactNativeHost::evaluate(const String &p_code) {
	Ref<RNEvaluation> evaluation;
	evaluation.instantiate();
	const uint64_t id = next_evaluation++;
	evaluations[id] = evaluation;
	const Signal completed(evaluation.ptr(), "completed");
	if (!_start()) {
		callable_mp(this, &ReactNativeHost::_finish_evaluation).call_deferred(id, Variant());
		return completed;
	}
	instance->evaluate(p_code, [this, id](const Variant &p_result) { _finish_evaluation(id, p_result); });
	return completed;
}

void ReactNativeHost::_finish_evaluation(uint64_t p_id, const Variant &p_result) {
	const Ref<RNEvaluation> *pending = evaluations.getptr(p_id);
	if (!pending) {
		return;
	}
	const Ref<RNEvaluation> evaluation = *pending;
	evaluations.erase(p_id);
	evaluation->emit_signal("completed", p_result);
}

void ReactNativeHost::shutdown() {
	_retire();
	_sync_back_press();
	for (const String &path : scene_loads) {
		ResourceLoader::load_threaded_get(path);
	}
	scene_loads.clear();
	for (const std::shared_ptr<RNHostInstance> &retired : retiring) {
		retired->finish();
	}
	retiring.clear();
	if (frame_connected) {
		if (SceneTree *tree = SceneTree::get_singleton()) {
			tree->disconnect("process_frame", callable_mp(this, &ReactNativeHost::_process_frame));
		}
		frame_connected = false;
	}
	evaluations.clear();
	roots.clear();
}
