#pragma once

#include "rn_host_descriptor.h"

#include "scene/gui/check_button.h"
#include "scene/gui/progress_bar.h"

class RNSwitchControl : public CheckButton {
	GDCLASS(RNSwitchControl, CheckButton);
	RNHostContext published_context;
	void _changed(bool p_value);

protected:
	static void _bind_methods() {}

public:
	RNSwitchControl();
	void publish(const RNHostContext &p_context) { published_context = p_context; }
};

class RNActivityIndicatorControl : public ProgressBar {
	GDCLASS(RNActivityIndicatorControl, ProgressBar);

protected:
	static void _bind_methods() {}

public:
	RNActivityIndicatorControl();
};

std::shared_ptr<const RNHostDescriptor> rn_switch_descriptor();
std::shared_ptr<const RNHostDescriptor> rn_activity_indicator_descriptor();
