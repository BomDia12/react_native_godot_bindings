#include "rn_mounting_manager.h"

#include "../host/rn_main_thread_queue.h"

#include <react/renderer/scheduler/Scheduler.h>

using namespace facebook::react;

RNMountingManager::RNMountingManager(std::shared_ptr<RNMainThreadQueue> p_main_thread, std::shared_ptr<const RNComponentViewFactories> p_factories, ComponentRegistryFactory p_registry_factory, std::shared_ptr<IImageLoader> p_image_loader) :
		main_thread(std::move(p_main_thread)), factories(std::move(p_factories)), registry_factory(std::move(p_registry_factory)), image_loader(std::move(p_image_loader)) {
}

void RNMountingManager::register_surface(SurfaceId p_surface, const RNMountedSurface &p_mounted) {
	surfaces[p_surface] = p_mounted;
}

void RNMountingManager::unregister_surface(SurfaceId p_surface) {
	surfaces.erase(p_surface);
}

void RNMountingManager::executeMount(SurfaceId p_surface, MountingTransaction &&p_transaction) {
	std::lock_guard lock(mutex);
	pending[p_surface].push_back(std::move(p_transaction));
	if (!flush_scheduled) {
		flush_scheduled = true;
		main_thread->post([this] { _flush(); });
	}
}

void RNMountingManager::_flush() {
	std::map<SurfaceId, std::vector<MountingTransaction>> transactions;
	SchedulerTaskExecutor executor;
	{
		std::lock_guard lock(mutex);
		transactions.swap(pending);
		flush_scheduled = false;
		executor = scheduler_executor;
	}
	for (const auto &[surface, queue] : transactions) {
		auto mounted = surfaces.find(surface);
		if (mounted == surfaces.end()) {
			continue;
		}
		for (const MountingTransaction &transaction : queue) {
			mounted->second.views->apply(transaction.getMutations());
		}
		if (mounted->second.after_mount) {
			mounted->second.after_mount();
		}
		if (executor && js_thread_runner) {
			js_thread_runner([executor, surface] { executor([surface](Scheduler &p_scheduler) { p_scheduler.reportMount(surface); }); });
		}
	}
}

RNComponentView *RNMountingManager::_find(Tag p_tag) const {
	for (const auto &[surface, mounted] : surfaces) {
		if (RNComponentView *view = mounted.views->find(p_tag)) {
			return view;
		}
	}
	return nullptr;
}

void RNMountingManager::dispatchCommand(const ShadowView &p_view, const std::string &p_command, const folly::dynamic &p_args) {
	main_thread->post([this, surface = p_view.surfaceId, tag = p_view.tag, p_command, p_args] {
		auto mounted = surfaces.find(surface);
		RNComponentView *view = mounted == surfaces.end() ? nullptr : mounted->second.views->find(tag);
		if (view) {
			view->handle_command(p_command, p_args);
		}
	});
}

void RNMountingManager::synchronouslyUpdateViewOnUIThread(Tag p_tag, const folly::dynamic &p_props) {
	auto apply = [this, p_tag, p_props] {
		if (RNComponentView *view = _find(p_tag)) {
			view->synchronously_update_props(p_props);
		}
	};
	if (main_thread->is_main_thread()) {
		apply();
	} else {
		main_thread->post(std::move(apply));
	}
}

void RNMountingManager::setSchedulerTaskExecutor(SchedulerTaskExecutor &&p_executor) noexcept {
	std::lock_guard lock(mutex);
	scheduler_executor = std::move(p_executor);
}
