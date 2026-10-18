#pragma once

#include "rn_service_settings.h"

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

// Completes once with the value a ReactNativeHost.evaluate() call produced.
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
	ObjectID lifecycle_node;
	bool frame_connected = false;
	bool processing_frame = false;
	std::deque<std::function<void()>> deferred;
	HashMap<uint64_t, Ref<RNEvaluation>> evaluations;
	uint64_t next_evaluation = 1;

	mutable std::mutex error_mutex;
	String last_error;
	uint64_t error_generation = 0;

	bool _start();
	void _retire();
	void _finish_retired();
	void _start_surface(ReactNativeRootView *p_root);
	void _stop_surface(ReactNativeRootView *p_root, bool p_stop_react = true);
	void _process_frame();
	void _theme_changed();
	void _bundle_changed(const String &p_path, const String &p_content, bool p_exists);
	void _record_error(uint64_t p_generation, const String &p_error);
	void _finish_evaluation(uint64_t p_id, const Variant &p_result);
	bool _defer(std::function<void()> p_task);

protected:
	static void _bind_methods();

public:
	ReactNativeHost();
	~ReactNativeHost() override;
	static ReactNativeHost *get_singleton() { return singleton; }

	Signal evaluate(const String &p_code);
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
	ReactNativeRootView *find_root(facebook::react::SurfaceId p_surface) const;
	void shutdown();
};
