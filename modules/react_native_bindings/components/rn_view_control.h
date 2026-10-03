#pragma once

#include "rn_host_descriptor.h"

#include "scene/gui/panel.h"
#include "scene/gui/popup_menu.h"

class RNViewControl : public Panel {
	GDCLASS(RNViewControl, Panel);

	PopupMenu *menu = nullptr;
	Array published_entries;
	RNHostContext published_context;
	void _menu_selected(int p_id);

protected:
	static void _bind_methods() {}
	void gui_input(const Ref<InputEvent> &p_event) override;

public:
	void publish_menu(const RNPreparedHostState &p_state, const RNHostContext &p_context);
};

std::shared_ptr<const RNHostDescriptor> rn_view_descriptor();
