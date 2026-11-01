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
#include <optional>
#include <vector>

namespace facebook::react {
class RNSourceURL;
class RNWebSocketHub;
class RunLoopObserverManager;
} // namespace facebook::react

class ReactNativeRootView;
struct RNRootState;
class RNBlobService;
class RNDevServer;
class RNGodotApplication;
class RNGodotHostEvents;
class RNHTTPService;
class RNImageService;
class RNLogBoxSurface;
class RNMainThreadQueue;
class RNMountingManager;
class RNSceneBindings;
struct RNSceneAttachment;
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
		std::function<std::shared_ptr<const RNSceneAttachment>(const String &p_name)> find_module;
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
	std::shared_ptr<RNGodotHostEvents> host_events;
	std::shared_ptr<RNMountingManager> mounting;
	std::shared_ptr<facebook::react::RunLoopObserverManager> run_loop_observers;
	std::unique_ptr<RNDevServer> dev_server;
	std::shared_ptr<facebook::react::RNSourceURL> source_url;
	std::shared_ptr<RNLogBoxSurface> log_box;
	std::mutex animation_mutex;
	std::function<void()> animation_frame;
	bool suspended = false;
	// Segments evaluate in order after the bundle; ones requested earlier wait for it.
	struct Segment {
		String file;
		std::string source_url;
		std::function<void(bool)> done;
	};
	PackedStringArray startup_segments;
	String bundle_fingerprint;
	bool script_loaded = false;
	std::vector<Segment> waiting_segments;
	RNHostRuntime runtime;
	std::unique_ptr<RNHostTeardown> teardown;

	void _on_js_thread(std::function<void()> p_task, facebook::react::SchedulerPriority p_priority = facebook::react::SchedulerPriority::ImmediatePriority);
	void _report_error(const String &p_error);
	std::vector<Segment> _segment_files(const String &p_bundle) const;
	void _download_segments(const String &p_bundle, std::vector<Segment> p_downloaded, std::function<void(std::vector<Segment>)> p_done);
	void _bundle_loaded(const std::vector<Segment> &p_startup);
	void _evaluate_segment(const Segment &p_segment);
	void _run_animation_frame();

public:
	explicit RNHostInstance(const Setup &p_setup);
	~RNHostInstance();

	bool load_script(const String &p_bundle);
	void load_segment(const String &p_path, std::function<void(bool p_ok)> p_done);
	void start_surface(ReactNativeRootView *p_root, facebook::react::SurfaceId p_surface);
	void stop_surface(ReactNativeRootView *p_root, bool p_stop_react);
	void update_layout(ReactNativeRootView *p_root);
	void scene_binding_changed(facebook::react::SurfaceId p_surface);
	void module_changed(const String &p_name);
	void set_paused(bool p_paused);
	void set_suspended(bool p_suspended);
	void root_state_changed(facebook::react::SurfaceId p_surface, const RNRootState &p_state);
	bool handles_back_press() const;
	void send_message(std::optional<int> p_root_tag, const String &p_name, const Variant &p_payload);
	void back_pressed();
	void set_fast_refresh(bool p_enabled);
	void evaluate(const String &p_code, std::function<void(const Variant &p_result)> p_done);
	Dictionary stats() const;
	void open_debugger();
	void process_frame();

	void retire(std::vector<facebook::react::SurfaceId> p_surfaces);
	bool is_finished() const;
	void finish();
};
