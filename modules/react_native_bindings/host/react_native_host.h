#pragma once

#include "../interop/rn_scene_binding.h"
#include "rn_service_settings.h"

#include "core/input/input_event.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"
#include "scene/main/node.h"

#include <react/renderer/core/ReactPrimitives.h>

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

class ReactNativeRootView;
class RNHostInstance;
class RNThemeMetricsStore;

// Forwards application pause and resume, which only nodes receive.
class RNHostLifecycle : public Node {
	GDCLASS(RNHostLifecycle, Node);

protected:
	static void _bind_methods() {}
	void _notification(int p_what);
};

// Completes once with the value a ReactNativeHost.evaluate() call produced,
// or whether a load_segment() call succeeded.
class RNEvaluation : public RefCounted {
	GDCLASS(RNEvaluation, RefCounted);

protected:
	static void _bind_methods();
};

// Runs React Native for every ReactNativeRootView: one RNHostInstance with its
// upstream ReactHost, JS thread and Godot services, and the per-frame work that
// applies JS results on the main thread. A hard reload replaces the instance and
// tears the old one down in the background.
class ReactNativeHost : public Object {
	GDCLASS(ReactNativeHost, Object);

	static ReactNativeHost *singleton;

	std::shared_ptr<RNThemeMetricsStore> theme;
	RNServiceSettings settings;
	std::shared_ptr<RNHostInstance> instance;
	Vector<std::shared_ptr<RNHostInstance>> retiring;

	Vector<ObjectID> roots;
	HashMap<facebook::react::SurfaceId, ObjectID> surfaces;
	facebook::react::SurfaceId next_surface_id = 1;
	uint64_t generation = 0;
	uint64_t theme_revision = 0;
	ObjectID lifecycle_node;
	bool frame_connected = false;
	bool processing_frame = false;
	std::deque<std::function<void()>> deferred;
	HashMap<uint64_t, Ref<RNEvaluation>> evaluations;
	Vector<String> loaded_segments;
	uint64_t next_evaluation = 1;
	HashMap<String, std::shared_ptr<const RNSceneAttachment>> modules;
	uint64_t next_module_identity = 1;
	bool takes_back_press = false;
	bool project_quits_on_go_back = true;
	bool back_press_quits = false;
	Ref<InputEventAction> default_back_press;

	mutable std::mutex error_mutex;
	String last_error;
	uint64_t error_generation = 0;

	bool _start();
	void _retire();
	void _sync_back_press();
	void _finish_retired();
	void _start_surface(ReactNativeRootView *p_root);
	void _publish_theme(ReactNativeRootView *p_root, facebook::react::SurfaceId p_surface);
	void _stop_surface(ReactNativeRootView *p_root, bool p_stop_react = true);
	void _process_frame();
	void _theme_changed();
	void _bundle_changed(const String &p_path, const String &p_content, bool p_exists);
	void _record_error(uint64_t p_generation, const String &p_error);
	void _finish_evaluation(uint64_t p_id, const Variant &p_result);
	void _module_node_moved(const String &p_name, uint64_t p_identity);
	bool _defer(std::function<void()> p_task);
	void _update_suspension();
	void _settings_changed();

protected:
	static void _bind_methods();

public:
	ReactNativeHost();
	~ReactNativeHost() override;
	static ReactNativeHost *get_singleton() { return singleton; }

	Signal evaluate(const String &p_code);
	Signal load_segment(const String &p_path);
	uint64_t get_generation() const { return generation; }
	String get_last_error() const;
	Dictionary get_service_stats() const;
	void reload();
	void open_debugger();
	void set_application_paused(bool p_paused);

	void register_root(ReactNativeRootView *p_root);
	void unregister_root(ReactNativeRootView *p_root);
	void restart_root(ReactNativeRootView *p_root);
	void update_root_layout(ReactNativeRootView *p_root);
	void scene_binding_changed(facebook::react::SurfaceId p_surface);
	void root_state_changed(ReactNativeRootView *p_root);
	void post_message(const String &p_name, const Variant &p_payload);
	Dictionary register_module(const String &p_name, Object *p_target, const Ref<RNSceneBinding> &p_binding);
	void unregister_module(const String &p_name);
	std::shared_ptr<const RNSceneAttachment> find_module(const String &p_name) const;
	void send_message(std::optional<int> p_root_tag, const String &p_name, const Variant &p_payload);
	bool handles_back_press() const;
	void back_pressed(bool p_go_back_request);
	void invoke_default_back_press();
	bool is_default_back_press(const Ref<InputEvent> &p_event) const { return p_event.ptr() == default_back_press.ptr(); }
	ReactNativeRootView *find_root(facebook::react::SurfaceId p_surface) const;
	void shutdown();
};
