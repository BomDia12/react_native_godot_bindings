#include "rn_dev_settings_module.h"

#include "../host/rn_main_thread_queue.h"

namespace facebook::react {

RNDevSettingsModule::RNDevSettingsModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_context) :
		NativeDevSettingsCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
}

void RNDevSettingsModule::reload(jsi::Runtime &rt) {
	reloadWithReason(rt, "");
}

void RNDevSettingsModule::reloadWithReason(jsi::Runtime &, const std::string &) {
	if (context.reload) {
		context.main_thread->post(context.reload);
	}
}

void RNDevSettingsModule::openDebugger(jsi::Runtime &) {
	if (context.open_debugger) {
		context.main_thread->post(context.open_debugger);
	}
}

} // namespace facebook::react
