#include "rn_accessibility_module.h"

#include "../host/rn_main_thread_queue.h"
#include "rn_godot_application.h"

#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

namespace facebook::react {

RNAccessibilityManagerModule::RNAccessibilityManagerModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_host, RNApplicationModuleContext p_application) :
		NativeAccessibilityManagerCxxSpec(std::move(jsInvoker)), host(std::move(p_host)), application(std::move(p_application)) {
}

void RNAccessibilityManagerModule::getCurrentBoldTextState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function) {
	onSuccess.call(rt, false);
}

void RNAccessibilityManagerModule::getCurrentGrayscaleState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function) {
	onSuccess.call(rt, false);
}

void RNAccessibilityManagerModule::getCurrentInvertColorsState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function) {
	onSuccess.call(rt, false);
}

void RNAccessibilityManagerModule::getCurrentReduceMotionState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function) {
	onSuccess.call(rt, false);
}

void RNAccessibilityManagerModule::getCurrentDarkerSystemColorsState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function) {
	onSuccess.call(rt, false);
}

void RNAccessibilityManagerModule::getCurrentPrefersCrossFadeTransitionsState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function) {
	onSuccess.call(rt, false);
}

void RNAccessibilityManagerModule::getCurrentReduceTransparencyState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function) {
	onSuccess.call(rt, false);
}

void RNAccessibilityManagerModule::getCurrentVoiceOverState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function) {
	onSuccess.call(rt, application.application->get_snapshot().screen_reader);
}

void RNAccessibilityManagerModule::setAccessibilityFocus(jsi::Runtime &, double reactTag) {
	if (host.focus_view) {
		host.main_thread->post([focus = host.focus_view, tag = int(reactTag)] { focus(tag); });
	}
}

void RNAccessibilityManagerModule::announceForAccessibility(jsi::Runtime &rt, jsi::String announcement) {
	host.main_thread->post([text = String::utf8(announcement.utf8(rt).c_str())] {
		if (SceneTree *tree = SceneTree::get_singleton()) {
			tree->get_root()->accessibility_announcement(text);
		}
	});
}

void RNAccessibilityManagerModule::announceForAccessibilityWithOptions(jsi::Runtime &rt, jsi::String announcement, jsi::Object) {
	announceForAccessibility(rt, std::move(announcement));
}

} // namespace facebook::react
