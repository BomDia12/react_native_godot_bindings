#include "rn_host_instance.h"

#include "../components/rn_image_component_view.h"
#include "../components/rn_paragraph_component_view.h"
#include "../components/rn_presentation_component_views.h"
#include "../components/rn_scroll_component_view.h"
#include "../components/rn_small_component_views.h"
#include "../components/rn_text_input_component_view.h"
#include "../components/rn_text_shaper.h"
#include "../components/rn_theme_metrics.h"
#include "../components/rn_view_component_view.h"
#include "../examples/rn_example_meter_component_view.h"
#include "../examples/rn_example_scene_module.h"
#include "../interop/rn_value_codec.h"
#include "../mounting/rn_mounting_manager.h"
#include "../native_modules/rn_blob_service.h"
#include "../native_modules/rn_http_service.h"
#include "../native_modules/rn_image_service.h"
#include "../root_view/react_native_root_view.h"
#include "../turbomodules/rn_godot_alerts.h"
#include "../turbomodules/rn_godot_turbo_module_provider.h"
#include "../turbomodules/rn_image_loader.h"
#include "rn_dev_server.h"
#include "rn_inspector_socket.h"
#include "rn_log_box_surface.h"
#include "rn_main_thread_queue.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

#include <jsi/hermes-interfaces.h>
#include <logger/react_native_log.h>
#include <react/http/platform/godot/GodotNetworkClients.h>
#include <react/renderer/animated/NativeAnimatedNodesManagerProvider.h>
#include <react/renderer/components/FBReactNativeSpec/ComponentDescriptors.h>
#include <react/renderer/components/ReactNativeGodotSpec/ComponentDescriptors.h>
#include <react/renderer/components/godotwindow/GodotWindowShadowNode.h>
#include <react/renderer/components/image/ImageComponentDescriptor.h>
#include <react/renderer/components/modal/ModalHostViewComponentDescriptor.h>
#include <react/renderer/components/scrollview/ScrollViewComponentDescriptor.h>
#include <react/renderer/components/switch/GodotSwitchShadowNode.h>
#include <react/renderer/components/text/ParagraphComponentDescriptor.h>
#include <react/renderer/components/text/RawTextComponentDescriptor.h>
#include <react/renderer/components/text/TextComponentDescriptor.h>
#include <react/renderer/components/textinput/GodotTextInputComponentDescriptor.h>
#include <react/renderer/components/view/ViewComponentDescriptor.h>
#include <react/renderer/core/LayoutConstraints.h>
#include <react/runtime/ReactHost.h>
#include <react/runtime/platform/godot/GodotTimerRegistry.h>
#include <react/utils/RunLoopObserverManager.h>

using namespace facebook;
using namespace facebook::react;

namespace {

ComponentRegistryFactory component_registry_factory() {
	return [](const EventDispatcher::Weak &p_dispatcher, const std::shared_ptr<const ContextContainer> &p_context) {
		static const auto providers = [] {
			auto registry = std::make_shared<ComponentDescriptorProviderRegistry>();
			registry->add(concreteComponentDescriptorProvider<ViewComponentDescriptor>());
			registry->add(concreteComponentDescriptorProvider<ParagraphComponentDescriptor>());
			registry->add(concreteComponentDescriptorProvider<TextComponentDescriptor>());
			registry->add(concreteComponentDescriptorProvider<RawTextComponentDescriptor>());
			registry->add(concreteComponentDescriptorProvider<ImageComponentDescriptor>());
			registry->add(concreteComponentDescriptorProvider<ScrollViewComponentDescriptor>());
			registry->add(concreteComponentDescriptorProvider<ModalHostViewComponentDescriptor>());
			registry->add(concreteComponentDescriptorProvider<ActivityIndicatorViewComponentDescriptor>());
			registry->add(concreteComponentDescriptorProvider<UnimplementedNativeViewComponentDescriptor>());
			registry->add(concreteComponentDescriptorProvider<GodotTextInputComponentDescriptor>());
			registry->add(concreteComponentDescriptorProvider<GodotSwitchComponentDescriptor>());
			registry->add(concreteComponentDescriptorProvider<GodotWindowComponentDescriptor>());
			registry->add(concreteComponentDescriptorProvider<RNExampleMeterComponentDescriptor>());
			return registry;
		}();
		const ComponentDescriptorParameters parameters{ p_dispatcher, p_context, nullptr };
		auto registry = providers->createComponentDescriptorRegistry(parameters);
		std::const_pointer_cast<ComponentDescriptorRegistry>(registry)->setFallbackComponentDescriptor(std::make_shared<UnimplementedNativeViewComponentDescriptor>(parameters));
		return registry;
	};
}

std::shared_ptr<RNComponentViewFactories> component_view_factories(const std::shared_ptr<const RNTextShaper> &p_shaper, const std::shared_ptr<RNImageService> &p_images) {
	auto factories = std::make_shared<RNComponentViewFactories>();
	auto view = [](const ShadowView &p_view) { return std::make_unique<RNViewComponentView>(p_view.tag); };
	factories->add("View", view);
	factories->add("UnimplementedNativeView", view);
	factories->set_fallback(view);
	factories->add("Paragraph", [p_shaper](const ShadowView &p_view) { return std::make_unique<RNParagraphComponentView>(p_view.tag, p_shaper); });
	factories->add("Image", [p_images](const ShadowView &p_view) { return std::make_unique<RNImageComponentView>(p_view.tag, p_images); });
	factories->add("ScrollView", [](const ShadowView &p_view) { return std::make_unique<RNScrollComponentView>(p_view.tag); });
	factories->add(GodotTextInputComponentName, [p_shaper](const ShadowView &p_view) { return std::make_unique<RNTextInputComponentView>(p_view.tag, p_shaper); });
	factories->add(GodotSwitchComponentName, [](const ShadowView &p_view) { return std::make_unique<RNSwitchComponentView>(p_view.tag); });
	factories->add(ActivityIndicatorViewComponentName, [](const ShadowView &p_view) { return std::make_unique<RNActivityIndicatorComponentView>(p_view.tag); });
	factories->add("ModalHostView", [](const ShadowView &p_view) { return std::make_unique<RNModalComponentView>(p_view.tag); });
	factories->add(GodotWindowComponentName, [](const ShadowView &p_view) { return std::make_unique<RNWindowComponentView>(p_view.tag); });
	factories->add(RNExampleMeterComponentName, [](const ShadowView &p_view) { return std::make_unique<RNExampleMeterComponentView>(p_view.tag); });
	return factories;
}

void layout_for(ReactNativeRootView *p_root, const RNGodotApplication &p_application, LayoutConstraints &r_constraints, LayoutContext &r_context) {
	const Size2 size = p_root->get_size();
	r_constraints.minimumSize = { .width = size.x, .height = size.y };
	r_constraints.maximumSize = r_constraints.minimumSize;
	r_constraints.layoutDirection = p_root->is_layout_rtl() || p_application.forces_rtl() ? LayoutDirection::RightToLeft : LayoutDirection::LeftToRight;
	const Window *window = p_root->get_window();
	r_context.pointScaleFactor = window ? window->get_content_scale_factor() : 1;
	r_context.fontSizeMultiplier = double(GLOBAL_GET("react_native/text/font_scale"));
	const Point2 offset = p_root->get_global_position();
	r_context.viewportOffset = { .x = offset.x, .y = offset.y };
	r_context.swapLeftAndRightInRTL = p_application.get_snapshot().swap_left_and_right;
}

String processed_error(const JsErrorHandler::ProcessedError &p_error) {
	String result = String::utf8(p_error.message.c_str());
	for (const JsErrorHandler::ProcessedError::StackFrame &frame : p_error.stack) {
		result += vformat("\n    at %s (%s:%d:%d)", String::utf8(frame.methodName.c_str()), String::utf8(frame.file.value_or("").c_str()), frame.lineNumber.value_or(0), frame.column.value_or(0));
	}
	return result;
}

} // namespace

RNHostInstance::RNHostInstance(const Setup &p_setup) :
		on_error(p_setup.on_error) {
	SceneTree *tree = SceneTree::get_singleton();
	main_thread = std::make_shared<RNMainThreadQueue>();
	application = std::make_shared<RNGodotApplication>();
	// JavaScript reads dimensions, appearance and locale while the bundle loads.
	application->poll();
	blobs = std::make_shared<RNBlobService>(p_setup.settings.limit("binary/max_blob_bytes"));
	RNHTTPService *http_node = memnew(RNHTTPService(p_setup.settings));
	tree->get_root()->call_deferred("add_child", http_node, false, Node::INTERNAL_MODE_BACK);
	http = std::shared_ptr<RNHTTPService>(http_node, [id = http_node->get_instance_id()](RNHTTPService *p_service) {
		if (ObjectDB::get_instance(id) != p_service) {
			return;
		}
		if (p_service->get_parent()) {
			p_service->get_parent()->remove_child(p_service);
		}
		memdelete(p_service);
	});
	images = RNImageService::for_generation(p_setup.generation);
	images->set_transport(rn_http_image_transport(http));
	websockets = std::make_shared<RNWebSocketHub>(blobs, p_setup.settings);
	shaper = std::make_shared<RNTextShaper>(p_setup.theme, main_thread);
	scenes = std::make_shared<RNSceneBindings>(p_setup.find_root);
	auto image_loader = std::make_shared<RNImageLoader>(images, main_thread);
	mounting = std::make_shared<RNMountingManager>(main_thread, component_view_factories(shaper, images), component_registry_factory(), image_loader);
	run_loop_observers = std::make_shared<RunLoopObserverManager>();
	log_box = std::make_shared<RNLogBoxSurface>(main_thread);
	if (p_setup.settings.dev_server.enabled) {
		dev_server = std::make_unique<RNDevServer>(p_setup.settings.dev_server);
	}
	const auto reload_from_main_thread = [queue = std::weak_ptr<RNMainThreadQueue>(main_thread), reload = p_setup.reload] {
		if (auto alive = queue.lock()) {
			alive->post(reload);
		}
	};
	mounting->set_js_thread_runner([this](std::function<void()> p_task) {
		if (!retired) {
			_on_js_thread(std::move(p_task));
		}
	});

	auto context = std::make_shared<ContextContainer>();
	context->insert(HttpClientFactoryKey, rn_http_client_factory(http, main_thread));
	context->insert(WebSocketClientFactoryKey, rn_web_socket_client_factory(main_thread));
	if (dev_server) {
		context->insert(DevToolsWebSocketClientFactoryKey, rn_inspector_web_socket_client_factory(rn_web_socket_client_factory(main_thread), reload_from_main_thread));
	}
	context->insert(RNTextShaper::CONTEXT_KEY, std::shared_ptr<const RNTextShaper>(shaper));
	context->insert(GodotSwitchComponentDescriptor::THEME_KEY, std::shared_ptr<const RNThemeMetricsStore>(p_setup.theme));

	RNTurboModuleContext modules;
	modules.host = { main_thread, [find_root = p_setup.find_root](int p_root_tag) -> Object * { return find_root(p_root_tag); }, p_setup.reload, [this] { open_debugger(); }, [weak = std::weak_ptr<RNMountingManager>(mounting)](int p_tag) {
						if (auto alive = weak.lock()) {
							alive->focus_view(p_tag);
						} } };
	modules.source_url = dev_server ? dev_server->bundle_url() : "";
	modules.application = { application, main_thread };
	modules.network = { blobs, http, websockets, main_thread };
	modules.scenes = scenes;
	modules.image_loader = image_loader;
	modules.extensions[std::string(RNExampleSceneModule::kModuleName)] = [](const std::shared_ptr<CallInvoker> &p_invoker, const RNHostModuleContext &p_host) {
		return std::make_shared<RNExampleSceneModule>(p_invoker, p_host);
	};

	ReactInstanceConfig config;
	config.enableDevMode = false;
	config.enableInspector = false;
	if (dev_server) {
		dev_server->configure(config);
	}
	config.platformTimerRegistryFactory = [this, context] {
		auto registry = std::make_unique<GodotTimerRegistry>(context);
		runtime.timers = registry.get();
		return std::unique_ptr<PlatformTimerRegistry>(std::move(registry));
	};
	auto js_error = [this](jsi::Runtime &, const JsErrorHandler::ProcessedError &p_error) {
		_report_error(processed_error(p_error));
	};
	auto logger = [](const std::string &p_message, unsigned int p_level) {
		const String message = String::utf8(p_message.c_str());
		if (p_level >= ReactNativeLogLevelError) {
			ERR_PRINT(message);
		} else if (p_level == ReactNativeLogLevelWarning) {
			WARN_PRINT(message);
		} else {
			print_line(message);
		}
	};
	auto install_bindings = [this, blob_service = blobs](jsi::Runtime &p_runtime) {
		runtime.hermes = jsi::castInterface<facebook::hermes::IHermes>(&p_runtime);
		jsi::Object provider = jsi::Object::createFromHostObject(p_runtime, blob_service->collector_provider());
		p_runtime.global().setProperty(p_runtime, "__blobCollectorProvider", provider.getProperty(p_runtime, "create"));
	};
	// Native Animated starts and stops its frame callback from the JS thread and
	// from inside a frame, so the callback is swapped under a lock and runs on
	// the main thread once per Godot frame.
	auto animated = std::make_shared<NativeAnimatedNodesManagerProvider>(
			[this](std::function<void()> &&p_on_render, bool) {
				std::lock_guard lock(animation_mutex);
				animation_frame = std::move(p_on_render);
			},
			[this](bool) {
				std::lock_guard lock(animation_mutex);
				animation_frame = nullptr;
			});
	runtime.react_host = std::make_unique<ReactHost>(config, mounting, run_loop_observers, context, js_error, logger, nullptr, TurboModuleProviders{ rn_godot_turbo_module_provider(modules) }, log_box, animated, install_bindings);
	if (dev_server) {
		dev_server->connect(rn_web_socket_client_factory(main_thread), reload_from_main_thread);
	}

	const auto emit = [this](const std::string &p_name, folly::dynamic p_payload) {
		main_thread->post([this, p_name, payload = std::move(p_payload)]() mutable {
			runtime.react_host->emitDeviceEvent(folly::dynamic::array(p_name, std::move(payload)));
		});
	};
	application->set_event_emitter(emit);
	websockets->set_event_emitter(emit);
}

RNHostInstance::~RNHostInstance() = default;

// A Metro bundle loads under its URL, which ResourceLoader resolves to the
// downloaded copy. Surfaces started meanwhile wait for the bundle in ReactHost.
bool RNHostInstance::load_script(const String &p_bundle) {
	if (!dev_server) {
		return runtime.react_host->loadScript(p_bundle.utf8().get_data(), "");
	}
	dev_server->download_bundle(*http, [this, p_bundle](bool p_downloaded) {
		if (retired) {
			return;
		}
		const bool loaded = p_downloaded ? runtime.react_host->loadScript(dev_server->bundle_url(), "") : FileAccess::exists(p_bundle) && runtime.react_host->loadScript(p_bundle.utf8().get_data(), "");
		if (!loaded) {
			const String error = vformat("React Native bundle '%s' could not be loaded from Metro or from disk.", p_bundle);
			on_error(error);
			ERR_PRINT(error);
		}
	});
	return true;
}

void RNHostInstance::_report_error(const String &p_error) {
	if (retired) {
		return;
	}
	on_error(p_error);
	ERR_PRINT("React Native JS error: " + p_error);
}

// Surface operations lock the shadow tree, which the JS thread holds while it
// lays out and may wait on the main thread for a font. Running them on the JS
// thread keeps the main thread from ever waiting on JavaScript.
void RNHostInstance::_on_js_thread(std::function<void()> p_task, SchedulerPriority p_priority) {
	runtime.react_host->runOnRuntimeScheduler([task = std::move(p_task)](jsi::Runtime &) { task(); }, p_priority);
}

void RNHostInstance::start_surface(ReactNativeRootView *p_root, SurfaceId p_surface) {
	mounting->register_surface(p_surface, p_root->_attach_surface(p_surface, mounting->get_factories()));
	LayoutConstraints constraints;
	LayoutContext context;
	layout_for(p_root, *application, constraints, context);
	_on_js_thread([host = runtime.react_host.get(), p_surface, key = std::string(p_root->get_application_key().utf8().get_data()), constraints, context] {
		host->startSurface(p_surface, key, folly::dynamic::object(), constraints, context);
	});
}

void RNHostInstance::stop_surface(ReactNativeRootView *p_root, bool p_stop_react) {
	const SurfaceId surface = p_root->get_root_tag();
	mounting->unregister_surface(surface);
	scenes->close_surface(surface);
	if (GodotAlerts *alerts = GodotAlerts::get_singleton()) {
		alerts->close_surface(surface);
	}
	p_root->_detach_surface();
	if (p_stop_react) {
		// A root's controls still report changes while it leaves the tree. Those
		// events and state updates hold the surface's components until the next
		// event beat, which never comes when the game quits, so flush them first.
		run_loop_observers->onRender();
		// React unmounts at higher priorities, so roots that leave together stop
		// one at a time, each after React has finished with the previous one.
		_on_js_thread([host = runtime.react_host.get(), surface] { host->stopSurface(surface); }, SchedulerPriority::LowPriority);
	}
}

void RNHostInstance::update_layout(ReactNativeRootView *p_root) {
	LayoutConstraints constraints;
	LayoutContext context;
	layout_for(p_root, *application, constraints, context);
	_on_js_thread([host = runtime.react_host.get(), surface = p_root->get_root_tag(), constraints, context] {
		host->setSurfaceConstraints(surface, constraints, context);
	});
}

void RNHostInstance::scene_binding_changed(SurfaceId p_surface) {
	scenes->binding_changed(p_surface);
}

void RNHostInstance::set_paused(bool p_paused) {
	application->set_paused(p_paused);
}

void RNHostInstance::evaluate(const String &p_code, std::function<void(const Variant &)> p_done) {
	runtime.react_host->runOnRuntimeScheduler([this, code = std::string(p_code.utf8().get_data()), done = std::move(p_done)](jsi::Runtime &p_runtime) {
		Variant result;
		try {
			const jsi::Value value = p_runtime.evaluateJavaScript(std::make_shared<jsi::StringBuffer>(code), "godot://evaluate");
			const RNDecodedValue decoded = RNValueCodec::from_js(p_runtime, value, RNValueSchema::value(RNValueType::DYNAMIC), "ReactNativeHost.evaluate");
			result = decoded.ok() ? decoded.value : Variant();
		} catch (const jsi::JSError &p_error) {
			if (!retired) {
				on_error(String::utf8(p_error.getMessage().c_str()) + "\n" + String::utf8(p_error.getStack().c_str()));
			}
		} catch (const std::exception &p_error) {
			if (!retired) {
				on_error(String::utf8(p_error.what()));
			}
		}
		main_thread->post([done, result] { done(result); });
	});
}

Dictionary RNHostInstance::stats() const {
	Dictionary stats;
	stats["http"] = http->stats();
	stats["websocket_peers"] = int64_t(websockets->get_socket_count());
	stats["blob_count"] = int64_t(blobs->count());
	stats["blob_bytes"] = int64_t(blobs->used_bytes());
	return stats;
}

void RNHostInstance::open_debugger() {
	runtime.react_host->openDebugger();
}

void RNHostInstance::_run_animation_frame() {
	std::function<void()> frame;
	{
		std::lock_guard lock(animation_mutex);
		frame = animation_frame;
	}
	if (frame) {
		frame();
	}
}

void RNHostInstance::process_frame() {
	main_thread->drain();
	_run_animation_frame();
	application->poll();
	websockets->process_frame();
	http->process_requests();
	blobs->drain_releases();
	scenes->process_frame();
	run_loop_observers->onRender();
}

void RNHostInstance::retire(std::vector<SurfaceId> p_surfaces) {
	retired = true;
	main_thread->close();
	dev_server.reset();
	log_box->close();
	scenes->shutdown();
	websockets->shutdown();
	http->shutdown();
	if (GodotAlerts *alerts = GodotAlerts::get_singleton()) {
		alerts->clear_presenters();
	}
	teardown = std::make_unique<RNHostTeardown>(runtime, std::move(p_surfaces));
}

bool RNHostInstance::is_finished() const {
	return teardown && teardown->is_finished();
}

void RNHostInstance::finish() {
	teardown.reset();
	blobs->shutdown();
}
