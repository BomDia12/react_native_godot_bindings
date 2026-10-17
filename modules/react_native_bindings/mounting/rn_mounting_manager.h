#pragma once

#include "rn_component_view_registry.h"

#include <react/renderer/uimanager/IMountingManager.h>

#include <map>
#include <mutex>
#include <vector>

class RNMainThreadQueue;

struct RNMountedSurface {
	RNComponentViewRegistry *views = nullptr;
	std::function<void()> after_mount;
};

// Receives upstream mounting transactions on the JS thread and applies them to
// each surface's component views on the main thread, in commit order.
class RNMountingManager : public facebook::react::IMountingManager {
	std::shared_ptr<RNMainThreadQueue> main_thread;
	std::shared_ptr<const RNComponentViewFactories> factories;
	facebook::react::ComponentRegistryFactory registry_factory;
	std::shared_ptr<facebook::react::IImageLoader> image_loader;

	std::mutex mutex;
	std::map<facebook::react::SurfaceId, std::vector<facebook::react::MountingTransaction>> pending;
	bool flush_scheduled = false;
	facebook::react::SchedulerTaskExecutor scheduler_executor;
	std::function<void(std::function<void()>)> js_thread_runner;

	std::map<facebook::react::SurfaceId, RNMountedSurface> surfaces;

	void _flush();
	RNComponentView *_find(facebook::react::Tag p_tag) const;

public:
	RNMountingManager(std::shared_ptr<RNMainThreadQueue> p_main_thread, std::shared_ptr<const RNComponentViewFactories> p_factories, facebook::react::ComponentRegistryFactory p_registry_factory, std::shared_ptr<facebook::react::IImageLoader> p_image_loader);

	void register_surface(facebook::react::SurfaceId p_surface, const RNMountedSurface &p_mounted);
	void unregister_surface(facebook::react::SurfaceId p_surface);
	const std::shared_ptr<const RNComponentViewFactories> &get_factories() const { return factories; }
	// Mount reports lock the shadow tree, so they run on the JS thread.
	void set_js_thread_runner(std::function<void(std::function<void()>)> p_runner) { js_thread_runner = std::move(p_runner); }

	void executeMount(facebook::react::SurfaceId p_surface, facebook::react::MountingTransaction &&p_transaction) override;
	void dispatchCommand(const facebook::react::ShadowView &p_view, const std::string &p_command, const folly::dynamic &p_args) override;
	void synchronouslyUpdateViewOnUIThread(facebook::react::Tag p_tag, const folly::dynamic &p_props) override;
	facebook::react::ComponentRegistryFactory getComponentRegistryFactory() override { return registry_factory; }
	bool hasComponent(const std::string &p_name) override { return factories->has(p_name); }
	void setSchedulerTaskExecutor(facebook::react::SchedulerTaskExecutor &&p_executor) noexcept override;
	std::shared_ptr<facebook::react::IImageLoader> getImageLoader() noexcept override { return image_loader; }
};
