#pragma once

#include "rn_host_teardown.h"
#include "rn_service_settings.h"

#include "core/variant/variant.h"

#include <ReactCommon/SchedulerPriority.h>
#include <react/renderer/core/ReactPrimitives.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace facebook::react {
class RNWebSocketHub;
class RunLoopObserverManager;
} // namespace facebook::react

class ReactNativeRootView;
class RNBlobService;
class RNGodotApplication;
class RNHTTPService;
class RNImageService;
class RNMainThreadQueue;
class RNMountingManager;
class RNSceneBindings;
class RNTextShaper;
class RNThemeMetricsStore;

// One React Native runtime with the Godot services it reaches. ReactNativeHost
// replaces the instance on a hard reload: the retired instance stops reaching
// Godot at once and destroys its runtime on a thread of its own.
class RNHostInstance {
public:
	struct Setup {
		RNServiceSettings settings;
		std::shared_ptr<RNThemeMetricsStore> theme;
		uint64_t generation = 0;
		std::function<ReactNativeRootView *(facebook::react::SurfaceId p_surface)> find_root;
		std::function<void(const String &p_error)> on_error;
		std::function<void()> reload;
	};

private:
	std::function<void(const String &)> on_error;
	std::atomic<bool> retired = false;
	std::shared_ptr<RNMainThreadQueue> main_thread;
	std::shared_ptr<RNTextShaper> shaper;
	std::shared_ptr<RNGodotApplication> application;
	std::shared_ptr<RNBlobService> blobs;
	std::shared_ptr<RNHTTPService> http;
	std::shared_ptr<RNImageService> images;
	std::shared_ptr<facebook::react::RNWebSocketHub> websockets;
	std::shared_ptr<RNSceneBindings> scenes;
	std::shared_ptr<RNMountingManager> mounting;
	std::shared_ptr<facebook::react::RunLoopObserverManager> run_loop_observers;
	std::mutex animation_mutex;
	std::function<void()> animation_frame;
	RNHostRuntime runtime;
	std::unique_ptr<RNHostTeardown> teardown;

	void _on_js_thread(std::function<void()> p_task, facebook::react::SchedulerPriority p_priority = facebook::react::SchedulerPriority::ImmediatePriority);
	void _report_error(const String &p_error);
	void _run_animation_frame();

public:
	explicit RNHostInstance(const Setup &p_setup);
	~RNHostInstance();

	bool load_script(const String &p_bundle);
	void start_surface(ReactNativeRootView *p_root, facebook::react::SurfaceId p_surface);
	void stop_surface(ReactNativeRootView *p_root, bool p_stop_react);
	void update_layout(ReactNativeRootView *p_root);
	void scene_binding_changed(facebook::react::SurfaceId p_surface);
	void set_paused(bool p_paused);
	void evaluate(const String &p_code, std::function<void(const Variant &p_result)> p_done);
	Dictionary stats() const;
	void process_frame();

	void retire(std::vector<facebook::react::SurfaceId> p_surfaces);
	bool is_finished() const;
	void finish();
};
