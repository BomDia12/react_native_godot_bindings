#include "rn_godot_turbo_module_provider.h"

#include "../host/rn_main_thread_queue.h"
#include "../interop/rn_dynamic.h"
#include "rn_accessibility_module.h"
#include "rn_alert_modules.h"
#include "rn_dev_settings_module.h"
#include "rn_godot_alerts.h"

#include <react/devsupport/SourceCodeModule.h>
#include <react/io/ImageLoaderModule.h>

namespace facebook::react {

TurboModuleProvider rn_godot_turbo_module_provider(RNTurboModuleContext context) {
	auto factories = std::make_shared<std::map<std::string, RNTurboModuleFactory>>(std::move(context.extensions));
	auto add = [&factories](std::string_view p_name, std::function<std::shared_ptr<TurboModule>(const std::shared_ptr<CallInvoker> &)> p_factory) {
		factories->emplace(std::string(p_name), [p_factory](const std::shared_ptr<CallInvoker> &p_invoker, const RNHostModuleContext &) { return p_factory(p_invoker); });
	};
	const RNHostModuleContext host = context.host;
	const RNSceneModuleContext scene{ context.scenes, context.host.main_thread };
	const RNApplicationModuleContext application = context.application;
	const RNNetworkModuleContext network = context.network;
	add(RNPlatformConstantsModule::kModuleName, [](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNPlatformConstantsModule>(p_invoker); });
	add(RNDeviceInfoModule::kModuleName, [application](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNDeviceInfoModule>(p_invoker, application); });
	add(RNAppStateModule::kModuleName, [application](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNAppStateModule>(p_invoker, application); });
	add(RNAppearanceModule::kModuleName, [application](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNAppearanceModule>(p_invoker, application); });
	add(RNI18nManagerModule::kModuleName, [application](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNI18nManagerModule>(p_invoker, application); });
	add(RNKeyboardObserverModule::kModuleName, [](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNKeyboardObserverModule>(p_invoker); });
	add(RNClipboardModule::kModuleName, [application](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNClipboardModule>(p_invoker, application); });
	add(RNLinkingManagerModule::kModuleName, [application](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNLinkingManagerModule>(p_invoker, application); });
	add(RNVibrationModule::kModuleName, [application](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNVibrationModule>(p_invoker, application); });
	add(RNNetworkingModule::kModuleName, [network](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNNetworkingModule>(p_invoker, network); });
	add(RNWebSocketModule::kModuleName, [network](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNWebSocketModule>(p_invoker, network); });
	add(RNBlobModule::kModuleName, [network](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNBlobModule>(p_invoker, network); });
	add(RNFileReaderModule::kModuleName, [network](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNFileReaderModule>(p_invoker, network); });
	add(RNAlertManagerModule::kModuleName, [host](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNAlertManagerModule>(p_invoker, host); });
	add(RNGodotAlertModule::kModuleName, [host](const std::shared_ptr<CallInvoker> &p_invoker) {
		auto module = std::make_shared<RNGodotAlertModule>(p_invoker, host);
		// Registering through the instance's queue keeps a runtime that is being
		// torn down from presenting the next runtime's alerts.
		host.main_thread->post([weak = std::weak_ptr<RNGodotAlertModule>(module)] {
			GodotAlerts::get_singleton()->add_presenter([weak](const String &p_request, int p_root_tag, const Dictionary &p_payload) {
				auto alive = weak.lock();
				if (alive) {
					alive->present(p_request.utf8().get_data(), p_root_tag, rn_dynamic_from_variant(p_payload));
				}
				return alive != nullptr;
			});
		});
		return module;
	});
	add(RNGodotSceneModule::kModuleName, [scene](const std::shared_ptr<CallInvoker> &p_invoker) {
		auto module = std::make_shared<RNGodotSceneModule>(p_invoker, scene);
		scene.scenes->add_emitter([weak = std::weak_ptr<RNGodotSceneModule>(module)](const Variant &p_change) {
			auto alive = weak.lock();
			if (alive) {
				alive->emit_changed(p_change);
			}
			return alive != nullptr;
		});
		return module;
	});
	add(RNGodotHostModule::kModuleName, [host, events = context.host_events](const std::shared_ptr<CallInvoker> &p_invoker) {
		auto module = std::make_shared<RNGodotHostModule>(p_invoker, host, events);
		events->add_listener([weak = std::weak_ptr<RNGodotHostModule>(module)](int p_root_tag, const RNRootState &p_state) {
			auto alive = weak.lock();
			if (alive) {
				alive->emit_root_state(p_root_tag, p_state);
			}
			return alive != nullptr;
		});
		return module;
	});
	add(RNAccessibilityManagerModule::kModuleName, [host, application](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNAccessibilityManagerModule>(p_invoker, host, application); });
	add(RNDevSettingsModule::kModuleName, [host](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<RNDevSettingsModule>(p_invoker, host); });
	add(SourceCodeModule::kModuleName, [url = context.source_url](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<SourceCodeModule>(p_invoker, url->get()); });
	add(ImageLoaderModule::kModuleName, [loader = std::weak_ptr<IImageLoader>(context.image_loader)](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<ImageLoaderModule>(p_invoker, loader); });
	return [factories, host](const std::string &p_name, const std::shared_ptr<CallInvoker> &p_invoker) -> std::shared_ptr<TurboModule> {
		auto factory = factories->find(p_name);
		return factory == factories->end() ? nullptr : factory->second(p_invoker, host);
	};
}

} // namespace facebook::react
