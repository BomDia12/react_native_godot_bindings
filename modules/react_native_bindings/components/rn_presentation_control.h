#pragma once
#include "rn_host_descriptor.h"

#include "scene/gui/panel.h"
#include "scene/main/window.h"

struct RNPresentationData : RNComponentData {
	Size2 viewport;
};
class RNModalOverlay : public Panel {
	GDCLASS(RNModalOverlay, Panel);

protected:
	static void _bind_methods() {}

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
};
class RNWindowContent : public Control {
	GDCLASS(RNWindowContent, Control);

protected:
	static void _bind_methods() {}

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
};
struct RNModalDomain {
	Vector<ObjectID> stack;
	ObjectID saved_focus;
	ObjectID container;
};
class RNModalControl : public Control {
	GDCLASS(RNModalControl, Control);
	RNModalOverlay *overlay = nullptr;
	ObjectID overlay_id;
	RNHostContext published_context;
	std::shared_ptr<RNModalDomain> domain;
	ObjectID last_focus;
	bool presented = false;
	void _remove_presentation();
	void _update_focus_cycle();

protected:
	static void _bind_methods() {}
	void input(const Ref<InputEvent> &p_event) override;

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	RNModalControl();
	~RNModalControl();
	RNModalOverlay *get_overlay() const { return overlay; }
	bool is_top() const;
	void publish(const RNPreparedHostState &p_state, const RNHostContext &p_context);
	void request_close();
	void dismiss() { _remove_presentation(); }
};
class RNWindowControl : public Control {
	GDCLASS(RNWindowControl, Control);
	Window *window = nullptr;
	RNWindowContent *content = nullptr;
	RNHostContext published_context;
	Size2 declared_size;
	bool publishing = false;
	void _close_requested();
	void _resized();

protected:
	static void _bind_methods() {}

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	RNWindowControl();
	Window *get_native_window() const { return window; }
	RNWindowContent *get_content() const { return content; }
	Size2 resolve_size(const Size2 &p_declared) const;
	void publish(const RNPreparedHostState &p_state, const RNHostContext &p_context);
};
std::shared_ptr<const RNHostDescriptor> rn_modal_descriptor();
std::shared_ptr<const RNHostDescriptor> rn_window_descriptor();
