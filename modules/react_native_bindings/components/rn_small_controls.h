#pragma once

#include "scene/gui/check_button.h"
#include "scene/gui/progress_bar.h"

#include <functional>
#include <memory>

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
