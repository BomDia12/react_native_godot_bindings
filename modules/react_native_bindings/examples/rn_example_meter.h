#pragma once

#include "../components/rn_host_descriptor_registry.h"

#include "scene/gui/progress_bar.h"

class RNExampleMeterControl : public ProgressBar {
	GDCLASS(RNExampleMeterControl, ProgressBar);

protected:
	static void _bind_methods() {}

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
};

bool rn_register_example_meter(RNHostDescriptorRegistry &p_registry, RNError &r_error);
