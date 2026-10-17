#pragma once

#include "scene/gui/progress_bar.h"

#include <functional>

class RNExampleMeterControl : public ProgressBar {
	GDCLASS(RNExampleMeterControl, ProgressBar);
	std::function<void(double)> value_changed;

protected:
	static void _bind_methods() {}
	void _value_changed(double p_value) override {
		if (value_changed) {
			value_changed(p_value);
		}
	}

public:
	void set_value_changed_callback(std::function<void(double)> p_callback) { value_changed = std::move(p_callback); }
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
};
