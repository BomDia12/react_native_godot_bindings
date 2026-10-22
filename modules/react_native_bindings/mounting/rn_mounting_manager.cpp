#include "rn_mounting_manager.h"

#include "../host/rn_main_thread_queue.h"

#include <react/renderer/runtimescheduler/RuntimeScheduler.h>
#include <react/renderer/scheduler/Scheduler.h>
#include <react/renderer/uimanager/UIManager.h>

using namespace facebook::react;

RNMountingManager::RNMountingManager(std::shared_ptr<RNMainThreadQueue> p_main_thread, std::shared_ptr<const RNComponentViewFactories> p_factories, ComponentRegistryFactory p_registry_factory, std::shared_ptr<IImageLoader> p_image_loader) :
		main_thread(std::move(p_main_thread)), factories(std::move(p_factories)), image_loader(std::move(p_image_loader)) {
	registry_factory = [this, inner = std::move(p_registry_factory)](const EventDispatcher::Weak &p_dispatcher, const std::shared_ptr<const ContextContainer> &p_context) {
		component_registry = inner(p_dispatcher, p_context);
		context_container = p_context;
		return component_registry;
	};
}

RNMountingManager::~RNMountingManager() {
	if (auto manager = ui_manager.lock()) {
		manager->setAnimationDelegate(nullptr);
	}
}

void RNMountingManager::setUIManager(std::weak_ptr<UIManager> p_ui_manager) noexcept {
	auto manager = p_ui_manager.lock();
	if (!manager || !context_container) {
		return;
	}
	auto runtime_scheduler = context_container->at<std::weak_ptr<RuntimeScheduler>>(RuntimeSchedulerKey);
	RuntimeExecutor runtime_executor = [runtime_scheduler](RawCallback &&p_callback) {
		if (auto scheduler = runtime_scheduler.lock()) {
			scheduler->scheduleWork(std::move(p_callback));
		}
	};
	layout_animations = std::make_shared<LayoutAnimationDriver>(runtime_executor, context_container, nullptr);
	layout_animations->setComponentDescriptorRegistry(component_registry);
	manager->setAnimationDelegate(layout_animations.get());
	manager->addOnSurfaceStartCallback([weak = std::weak_ptr<const MountingOverrideDelegate>(layout_animations)](const ShadowTree &p_tree) {
		p_tree.getMountingCoordinator()->setMountingOverrideDelegate(weak);
	});
	ui_manager = manager;
}

// The layout-animation half of UIManager::animationTick; native Animated runs
// from its own frame callback.
void RNMountingManager::tick_layout_animations() {
	auto manager = ui_manager.lock();
	if (manager && layout_animations && layout_animations->shouldAnimateFrame()) {
		manager->getShadowTreeRegistry().enumerate([](const ShadowTree &p_tree, bool &) { p_tree.notifyDelegatesOfUpdates(); });
	}
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

RNComponentView *RNMountingManager::_find(Tag p_tag, SurfaceId &r_surface) const {
	for (const auto &[surface, mounted] : surfaces) {
		if (RNComponentView *view = mounted.views->find(p_tag)) {
			r_surface = surface;
			return view;
		}
	}
	return nullptr;
}

void RNMountingManager::focus_view(Tag p_tag) {
	SurfaceId surface;
	RNComponentView *view = _find(p_tag, surface);
	Control *focus = view ? view->get_focus_control() : nullptr;
	if (focus && focus->is_inside_tree() && focus->get_focus_mode_with_override() != Control::FOCUS_NONE) {
		focus->grab_focus();
	}
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
		SurfaceId surface = 0;
		RNComponentView *view = _find(p_tag, surface);
		SchedulerTaskExecutor executor;
		{
			std::lock_guard lock(mutex);
			executor = scheduler_executor;
		}
		if (!view || !executor) {
			return;
		}
		executor([view, surface, &p_props](Scheduler &p_scheduler) {
			const ComponentDescriptor *descriptor = p_scheduler.findComponentDescriptorByHandle_DO_NOT_USE_THIS_IS_BROKEN(view->get_component_handle());
			if (descriptor) {
				view->synchronously_update_props(*descriptor, PropsParserContext{ surface, *p_scheduler.getContextContainer() }, p_props);
			}
		});
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
