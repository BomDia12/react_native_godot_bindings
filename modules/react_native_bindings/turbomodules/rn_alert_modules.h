#pragma once

#include "rn_host_module_context.h"

#include <FBReactNativeSpec/FBReactNativeSpecJSI.h>
#include <ReactNativeGodotSpec/ReactNativeGodotSpecJSI.h>

namespace facebook::react {

// Alerts presented on behalf of one root: natively, through the root's JS
// presenter, or by bubbling to Godot handlers.
class RNGodotAlertModule : public NativeGodotAlertCxxSpec<RNGodotAlertModule> {
	RNHostModuleContext context;

public:
	RNGodotAlertModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext context);
	jsi::Value request(jsi::Runtime &rt, double rootTag, jsi::Object payload, const std::string &mode);
	void reply(jsi::Runtime &rt, double rootTag, const std::string &requestId, jsi::Object result);
	void decline(jsi::Runtime &rt, double rootTag, const std::string &requestId);
	void present(const std::string &p_request, int p_root_tag, folly::dynamic p_payload);
};

// React Native's Alert.alert, presented by the application handler.
class RNAlertManagerModule : public NativeAlertManagerCxxSpec<RNAlertManagerModule> {
	RNHostModuleContext context;

public:
	RNAlertManagerModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext context);
	void alertWithArgs(jsi::Runtime &rt, jsi::Object args, AsyncCallback<double, std::string> callback);
};

} // namespace facebook::react
