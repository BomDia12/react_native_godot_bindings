#pragma once

#include "rn_host_descriptor.h"

#include "scene/gui/check_button.h"
#include "scene/gui/progress_bar.h"

class RNSwitchControl : public CheckButton {
	GDCLASS(RNSwitchControl, CheckButton);
	std::function<void(bool)> changed;
	void _changed(bool p_value);

protected:
	static void _bind_methods() {}

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	RNSwitchControl();
	void set_changed_callback(std::function<void(bool)> p_changed) { changed = std::move(p_changed); }
	void publish(const RNHostContext &p_context);
};

class RNActivityIndicatorControl : public ProgressBar {
	GDCLASS(RNActivityIndicatorControl, ProgressBar);

protected:
	static void _bind_methods() {}

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	RNActivityIndicatorControl();
};

std::shared_ptr<const RNHostDescriptor> rn_switch_descriptor();
std::shared_ptr<const RNHostDescriptor> rn_activity_indicator_descriptor();
