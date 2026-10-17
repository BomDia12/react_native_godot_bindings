#include "react_native_host.h"

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
#include "../singletons/react_native_file_singleton.h"
#include "../turbomodules/rn_godot_alerts.h"
#include "../turbomodules/rn_godot_turbo_module_provider.h"
#include "../turbomodules/rn_image_loader.h"
#include "rn_feature_flags.h"
#include "rn_logging.h"
#include "rn_main_thread_queue.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

#include <logger/react_native_log.h>
#include <react/http/platform/godot/GodotNetworkClients.h>
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
#include <react/utils/RunLoopObserverManager.h>

#include <future>
#include <mutex>

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

void layout_for(ReactNativeRootView *p_root, bool p_swap_left_and_right, LayoutConstraints &r_constraints, LayoutContext &r_context) {
	const Size2 size = p_root->get_size();
	r_constraints.minimumSize = { .width = size.x, .height = size.y };
	r_constraints.maximumSize = r_constraints.minimumSize;
	r_constraints.layoutDirection = p_root->is_layout_rtl() ? LayoutDirection::RightToLeft : LayoutDirection::LeftToRight;
	const Window *window = p_root->get_window();
	r_context.pointScaleFactor = window ? window->get_content_scale_factor() : 1;
	r_context.fontSizeMultiplier = double(GLOBAL_GET("react_native/text/font_scale"));
	const Point2 offset = p_root->get_global_position();
	r_context.viewportOffset = { .x = offset.x, .y = offset.y };
	r_context.swapLeftAndRightInRTL = p_swap_left_and_right;
}

String processed_error(const JsErrorHandler::ProcessedError &p_error) {
	String result = String::utf8(p_error.message.c_str());
	for (const JsErrorHandler::ProcessedError::StackFrame &frame : p_error.stack) {
		result += vformat("\n    at %s (%s:%d:%d)", String::utf8(frame.methodName.c_str()), String::utf8(frame.file.value_or("").c_str()), frame.lineNumber.value_or(0), frame.column.value_or(0));
	}
	return result;
}

} // namespace

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
	main_thread = std::make_shared<RNMainThreadQueue>();
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
}

void ReactNativeHost::_record_error(const String &p_error) {
	std::lock_guard lock(error_mutex);
	last_error = p_error;
}

String ReactNativeHost::get_last_error() const {
	std::lock_guard lock(error_mutex);
	return last_error;
}

Dictionary ReactNativeHost::get_service_stats() const {
	Dictionary stats;
	if (!react_host) {
		return stats;
	}
	stats["http"] = http->stats();
	stats["websocket_peers"] = int64_t(websockets->get_socket_count());
	stats["blob_count"] = int64_t(blobs->count());
	stats["blob_bytes"] = int64_t(blobs->used_bytes());
	return stats;
}

bool ReactNativeHost::_start() {
	if (react_host) {
		return true;
	}
	SceneTree *tree = SceneTree::get_singleton();
	ERR_FAIL_COND_V_MSG(!tree || !tree->get_root(), false, "React Native needs a SceneTree.");
	RNError error;
	if (!RNServiceSettings::snapshot(settings, error)) {
		_record_error(error.describe());
		ERR_PRINT(error.describe());
		return false;
	}
	ReactNativeFileSingleton *files = ReactNativeFileSingleton::get_singleton();
	const String bundle = files ? files->get_monitored_file() : String();
	if (!files || !files->has_file()) {
		_record_error(vformat("React Native bundle '%s' is missing.", bundle));
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
	_record_error(String());
	application = std::make_shared<RNGodotApplication>();
	blobs = std::make_shared<RNBlobService>(settings.limit("binary/max_blob_bytes"));
	RNHTTPService *http_node = memnew(RNHTTPService(settings));
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
	images = RNImageService::for_generation(generation);
	images->set_transport(rn_http_image_transport(http));
	websockets = std::make_shared<RNWebSocketHub>(blobs, settings);
	shaper = std::make_shared<RNTextShaper>(theme, main_thread);
	scenes = std::make_shared<RNSceneBindings>([this](int p_root_tag) { return find_root(p_root_tag); });
	auto image_loader = std::make_shared<RNImageLoader>(images, main_thread);
	mounting = std::make_shared<RNMountingManager>(main_thread, component_view_factories(shaper, images), component_registry_factory(), image_loader);
	run_loop_observers = std::make_shared<RunLoopObserverManager>();
	mounting->set_js_thread_runner([this](std::function<void()> p_task) {
		if (react_host) {
			_on_js_thread(std::move(p_task));
		}
	});
	configureGodotNetworkClients(http, main_thread);

	auto context = std::make_shared<ContextContainer>();
	context->insert(HttpClientFactoryKey, getHttpClientFactory());
	context->insert(WebSocketClientFactoryKey, getWebSocketClientFactory());
	context->insert(RNTextShaper::CONTEXT_KEY, std::shared_ptr<const RNTextShaper>(shaper));
	context->insert(GodotSwitchComponentDescriptor::THEME_KEY, std::shared_ptr<const RNThemeMetricsStore>(theme));

	RNTurboModuleContext modules;
	modules.host = { main_thread, [this](int p_root_tag) -> Object * { return find_root(p_root_tag); } };
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
	auto on_error = [this](jsi::Runtime &, const JsErrorHandler::ProcessedError &p_error) {
		const String message = processed_error(p_error);
		_record_error(message);
		ERR_PRINT("React Native JS error: " + message);
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
	auto install_bindings = [blob_service = blobs](jsi::Runtime &p_runtime) {
		jsi::Object provider = jsi::Object::createFromHostObject(p_runtime, blob_service->collector_provider());
		p_runtime.global().setProperty(p_runtime, "__blobCollectorProvider", provider.getProperty(p_runtime, "create"));
	};
	react_host = std::make_unique<ReactHost>(config, mounting, run_loop_observers, context, on_error, logger, nullptr, TurboModuleProviders{ rn_godot_turbo_module_provider(modules) }, nullptr, nullptr, install_bindings);

	const auto emit = [this](const std::string &p_name, folly::dynamic p_payload) {
		main_thread->post([this, p_name, payload = std::move(p_payload)]() mutable {
			if (react_host) {
				react_host->emitDeviceEvent(folly::dynamic::array(p_name, std::move(payload)));
			}
		});
	};
	application->set_event_emitter(emit);
	websockets->set_event_emitter(emit);

	if (!react_host->loadScript(bundle.utf8().get_data(), "")) {
		_record_error(vformat("React Native bundle '%s' could not be loaded.", bundle));
		ERR_PRINT(get_last_error());
	}
	return true;
}

void ReactNativeHost::_stop() {
	if (!react_host) {
		return;
	}
	for (const ObjectID &id : roots) {
		if (ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(id))) {
			_stop_surface(root, false);
		}
	}
	main_thread->begin_shutdown();
	// ReactHost stops every surface while it is destroyed, so surface work
	// already queued on the JS thread must finish first. Sync calls into Godot
	// fail during shutdown, so this wait cannot deadlock.
	auto idle = std::make_shared<std::promise<void>>();
	std::future<void> drained = idle->get_future();
	_on_js_thread([idle] { idle->set_value(); });
	drained.wait_for(std::chrono::seconds(5));
	react_host.reset();
	main_thread->drain();
	main_thread->end_shutdown();
	if (GodotAlerts *alerts = GodotAlerts::get_singleton()) {
		alerts->clear_presenters();
	}
	scenes->shutdown();
	websockets->shutdown();
	if (http) {
		http->shutdown();
	}
	blobs->shutdown();
	mounting.reset();
	run_loop_observers.reset();
	scenes.reset();
	websockets.reset();
	http.reset();
	images.reset();
	blobs.reset();
	shaper.reset();
	application.reset();
	for (const KeyValue<uint64_t, Ref<RNEvaluation>> &entry : evaluations) {
		callable_mp(this, &ReactNativeHost::_finish_evaluation).call_deferred(entry.key, Variant());
	}
}

void ReactNativeHost::_start_surface(ReactNativeRootView *p_root) {
	const SurfaceId surface = next_surface_id;
	next_surface_id += 10;
	surfaces[surface] = p_root->get_instance_id();
	mounting->register_surface(surface, p_root->_attach_surface(surface, mounting->get_factories()));
	LayoutConstraints constraints;
	LayoutContext context;
	layout_for(p_root, application->get_snapshot().swap_left_and_right, constraints, context);
	_on_js_thread([host = react_host.get(), surface, key = std::string(p_root->get_application_key().utf8().get_data()), constraints, context] {
		host->startSurface(surface, key, folly::dynamic::object(), constraints, context);
	});
}

void ReactNativeHost::_stop_surface(ReactNativeRootView *p_root, bool p_stop_react) {
	const SurfaceId surface = p_root->get_root_tag();
	if (surface == 0) {
		return;
	}
	if (react_host && p_stop_react) {
		_on_js_thread([host = react_host.get(), surface] { host->stopSurface(surface); });
	}
	if (mounting) {
		mounting->unregister_surface(surface);
	}
	if (scenes) {
		scenes->close_surface(surface);
	}
	if (GodotAlerts *alerts = GodotAlerts::get_singleton()) {
		alerts->close_surface(surface);
	}
	surfaces.erase(surface);
	p_root->_detach_surface();
}

// Surface operations lock the shadow tree, which the JS thread holds while it
// lays out and may wait on the main thread for a font. Running them on the JS
// thread keeps the main thread from ever waiting on JavaScript.
void ReactNativeHost::_on_js_thread(std::function<void()> p_task) {
	react_host->runOnRuntimeScheduler([task = std::move(p_task)](jsi::Runtime &) { task(); }, SchedulerPriority::ImmediatePriority);
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
	if (!react_host || p_root->get_root_tag() == 0) {
		return;
	}
	if (!roots.is_empty() && roots[0] == p_root->get_instance_id()) {
		theme->publish(RNThemeMetrics::capture(p_root, generation));
	}
	LayoutConstraints constraints;
	LayoutContext context;
	layout_for(p_root, application->get_snapshot().swap_left_and_right, constraints, context);
	_on_js_thread([host = react_host.get(), surface = p_root->get_root_tag(), constraints, context] {
		host->setSurfaceConstraints(surface, constraints, context);
	});
}

void ReactNativeHost::scene_binding_changed(SurfaceId p_surface) {
	if (scenes) {
		scenes->binding_changed(p_surface);
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
	_stop();
	if (!_start()) {
		return;
	}
	for (const ObjectID &id : roots) {
		if (ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(id))) {
			_start_surface(root);
		}
	}
}

void ReactNativeHost::_bundle_changed(const String &, const String &, bool) {
	if (react_host) {
		reload();
	}
}

void ReactNativeHost::set_application_paused(bool p_paused) {
	if (application) {
		application->set_paused(p_paused);
	}
}

void ReactNativeHost::_process_frame() {
	std::deque<std::function<void()>> pending;
	pending.swap(deferred);
	for (const std::function<void()> &task : pending) {
		task();
	}
	processing_frame = true;
	main_thread->drain();
	if (react_host) {
		application->poll();
		websockets->process_frame();
		http->process_requests();
		blobs->drain_releases();
		scenes->process_frame();
		run_loop_observers->onRender();
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
	react_host->runOnRuntimeScheduler([this, id, code = std::string(p_code.utf8().get_data()), queue = main_thread](jsi::Runtime &p_runtime) {
		Variant result;
		try {
			const jsi::Value value = p_runtime.evaluateJavaScript(std::make_shared<jsi::StringBuffer>(code), "godot://evaluate");
			const RNDecodedValue decoded = RNValueCodec::from_js(p_runtime, value, RNValueSchema::value(RNValueType::DYNAMIC), "ReactNativeHost.evaluate");
			result = decoded.ok() ? decoded.value : Variant();
		} catch (const jsi::JSError &p_error) {
			_record_error(String::utf8(p_error.getMessage().c_str()) + "\n" + String::utf8(p_error.getStack().c_str()));
		} catch (const std::exception &p_error) {
			_record_error(String::utf8(p_error.what()));
		}
		queue->post([this, id, result] { _finish_evaluation(id, result); });
	});
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
	_stop();
	main_thread->drain();
	if (frame_connected) {
		if (SceneTree *tree = SceneTree::get_singleton()) {
			tree->disconnect("process_frame", callable_mp(this, &ReactNativeHost::_process_frame));
		}
		frame_connected = false;
	}
	evaluations.clear();
	roots.clear();
}
