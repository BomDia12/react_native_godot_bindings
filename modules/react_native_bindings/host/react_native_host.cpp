#include "react_native_host.h"

#include "../components/rn_theme_metrics.h"
#include "../root_view/react_native_root_view.h"
#include "../singletons/react_native_file_singleton.h"
#include "rn_feature_flags.h"
#include "rn_host_instance.h"
#include "rn_logging.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

#include <mutex>

using namespace facebook::react;

void RNHostLifecycle::_notification(int p_what) {
	if ((p_what == NOTIFICATION_APPLICATION_PAUSED || p_what == NOTIFICATION_APPLICATION_RESUMED) && ReactNativeHost::get_singleton()) {
		ReactNativeHost::get_singleton()->set_application_paused(p_what == NOTIFICATION_APPLICATION_PAUSED);
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
	static std::once_flag process_setup;
	std::call_once(process_setup, [] {
		rn_install_logging();
		rn_install_feature_flags();
	});
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
			theme->publish(RNThemeMetrics::capture(theme_owner, generation + 1));
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
	setup.on_error = [this, current = generation](const String &p_error) { _record_error(current, p_error); };
	setup.reload = [this] { reload(); };
	instance = std::make_shared<RNHostInstance>(setup);
	if (!instance->load_script(bundle)) {
		_record_error(generation, vformat("React Native bundle '%s' could not be loaded.", bundle));
		ERR_PRINT(get_last_error());
	}
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
	instance->start_surface(p_root, surface);
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
}

void ReactNativeHost::unregister_root(ReactNativeRootView *p_root) {
	_stop_surface(p_root);
	roots.erase(p_root->get_instance_id());
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
		theme->publish(RNThemeMetrics::capture(p_root, generation));
	}
	instance->update_layout(p_root);
}

void ReactNativeHost::scene_binding_changed(SurfaceId p_surface) {
	if (instance) {
		instance->scene_binding_changed(p_surface);
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
