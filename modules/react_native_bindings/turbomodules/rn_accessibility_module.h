#pragma once

#include "rn_application_modules.h"
#include "rn_host_module_context.h"

#include <FBReactNativeSpec/FBReactNativeSpecJSI.h>

namespace facebook::react {

// React Native's iOS-shaped AccessibilityManager, which AccessibilityInfo uses
// on every platform but Android. Announcements go through Godot's own window
// announcement, and Godot exposes no bold-text, grayscale, inverted-color,
// reduced-motion, darker-color, cross-fade or reduced-transparency setting.
class RNAccessibilityManagerModule : public NativeAccessibilityManagerCxxSpec<RNAccessibilityManagerModule> {
	RNHostModuleContext host;
	RNApplicationModuleContext application;

public:
	RNAccessibilityManagerModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext host, RNApplicationModuleContext application);
	void getCurrentBoldTextState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function onError);
	void getCurrentGrayscaleState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function onError);
	void getCurrentInvertColorsState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function onError);
	void getCurrentReduceMotionState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function onError);
	void getCurrentDarkerSystemColorsState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function onError);
	void getCurrentPrefersCrossFadeTransitionsState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function onError);
	void getCurrentReduceTransparencyState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function onError);
	void getCurrentVoiceOverState(jsi::Runtime &rt, jsi::Function onSuccess, jsi::Function onError);
	// Font scaling belongs to the react_native/text/font_scale project setting.
	void setAccessibilityContentSizeMultipliers(jsi::Runtime &rt, jsi::Object JSMultipliers) {}
	void setAccessibilityFocus(jsi::Runtime &rt, double reactTag);
	void announceForAccessibility(jsi::Runtime &rt, jsi::String announcement);
	void announceForAccessibilityWithOptions(jsi::Runtime &rt, jsi::String announcement, jsi::Object options);
};

} // namespace facebook::react
