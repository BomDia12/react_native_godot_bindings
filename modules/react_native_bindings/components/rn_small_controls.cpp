#include "rn_small_controls.h"

#include "core/object/callable_mp.h"

RNSwitchControl::RNSwitchControl() {
	connect("toggled", callable_mp(this, &RNSwitchControl::_changed));
}
void RNSwitchControl::_changed(bool p_value) {
	if (changed) {
		changed(p_value);
	}
}
RNActivityIndicatorControl::RNActivityIndicatorControl() {
	set_show_percentage(false);
	set_indeterminate(true);
	set_mouse_filter(MOUSE_FILTER_IGNORE);
}
