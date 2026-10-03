#include "rn_builtin_descriptors.h"

#include "../fabric/rn_view_style.h"
#include "../root_view/react_native_root_view.h"
#include "rn_image_control.h"
#include "rn_presentation_control.h"
#include "rn_scroll_control.h"
#include "rn_small_controls.h"
#include "rn_text_control.h"
#include "rn_text_input_control.h"
#include "rn_view_control.h"

#include "core/object/callable_mp.h"
#include "scene/gui/label.h"
#include "scene/gui/panel.h"
#include "scene/resources/font.h"
#include "scene/theme/theme_db.h"

namespace {

class RNRootDescriptor : public RNHostDescriptor {
public:
	RNRootDescriptor() :
			RNHostDescriptor("RCTRootView", RNHostTraits{ false, true, true, false, false, false, false, false, true, false, true }) {}
};

class RNRawTextDescriptor : public RNHostDescriptor {
public:
	RNRawTextDescriptor() :
			RNHostDescriptor("RCTRawText", RNHostTraits{ false, false, false, false, true, false, false, false, false, false, true }) {}
};

} // namespace

bool rn_register_builtin_descriptors(RNHostDescriptorRegistry &p_registry, RNError &r_error) {
	return p_registry.register_descriptor(std::make_shared<RNRootDescriptor>(), r_error) &&
			p_registry.register_descriptor(rn_view_descriptor(), r_error) &&
			p_registry.register_descriptor(rn_modal_descriptor(), r_error) &&
			p_registry.register_descriptor(rn_window_descriptor(), r_error) &&
			p_registry.register_descriptor(rn_image_descriptor(), r_error) &&
			p_registry.register_descriptor(rn_switch_descriptor(), r_error) &&
			p_registry.register_descriptor(rn_activity_indicator_descriptor(), r_error) &&
			p_registry.register_descriptor(rn_scroll_descriptor(), r_error) &&
			p_registry.register_descriptor(rn_scroll_content_descriptor("RCTScrollContentView"), r_error) &&
			p_registry.register_descriptor(rn_text_input_descriptor(), r_error) &&
			p_registry.register_descriptor(rn_text_descriptor(), r_error) &&
			p_registry.register_descriptor(rn_virtual_text_descriptor(), r_error) &&
			p_registry.register_descriptor(std::make_shared<RNRawTextDescriptor>(), r_error);
}
