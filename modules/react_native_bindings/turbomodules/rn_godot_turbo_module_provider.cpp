#include "rn_godot_turbo_module_provider.h"

#include <react/io/ImageLoaderModule.h>

namespace facebook::react {

TurboModuleProvider rn_godot_turbo_module_provider(RNTurboModuleContext context) {
	auto factories = std::make_shared<std::map<std::string, RNTurboModuleFactory>>(std::move(context.extensions));
	auto add = [&factories](std::string_view p_name, RNTurboModuleFactory p_factory) {
		factories->emplace(std::string(p_name), std::move(p_factory));
	};
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
	add(ImageLoaderModule::kModuleName, [loader = std::weak_ptr<IImageLoader>(context.image_loader)](const std::shared_ptr<CallInvoker> &p_invoker) { return std::make_shared<ImageLoaderModule>(p_invoker, loader); });
	return [factories](const std::string &p_name, const std::shared_ptr<CallInvoker> &p_invoker) -> std::shared_ptr<TurboModule> {
		auto factory = factories->find(p_name);
		return factory == factories->end() ? nullptr : factory->second(p_invoker);
	};
}

} // namespace facebook::react
