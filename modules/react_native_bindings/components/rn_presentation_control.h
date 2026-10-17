#pragma once
#include "scene/gui/control.h"
#include "scene/gui/panel.h"
#include "scene/main/window.h"

#include <functional>
#include <memory>

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
struct RNPresentationCallbacks {
	std::function<void()> show;
	std::function<void()> dismiss;
	std::function<void()> request_close;
	std::function<void()> presented;
	std::function<void()> geometry_changed;
	std::function<void(const Size2 &)> resized;
};
class RNModalControl : public Control {
	GDCLASS(RNModalControl, Control);
	RNModalOverlay *overlay = nullptr;
	ObjectID overlay_id;
	RNPresentationCallbacks callbacks;
	std::shared_ptr<RNModalDomain> domain;
	ObjectID last_focus;
	bool presented = false;
	bool requested_visible = false;
	Color requested_backdrop;
	void _apply_request();
	void _remove_presentation();
	void _update_focus_cycle();

protected:
	static void _bind_methods() {}
	void input(const Ref<InputEvent> &p_event) override;
	void _notification(int p_what);

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	RNModalControl();
	~RNModalControl();
	RNModalOverlay *get_overlay() const { return overlay; }
	bool is_top() const;
	void set_callbacks(RNPresentationCallbacks p_callbacks) { callbacks = std::move(p_callbacks); }
	void present(bool p_visible, const Color &p_backdrop, const Size2 &p_viewport);
	// Presents over the current viewport once the modal is in the tree.
	void request_presentation(bool p_visible, const Color &p_backdrop);
	Size2 get_presentation_size() const;
	void request_close();
	void dismiss() { _remove_presentation(); }
};
class RNWindowControl : public Control {
	GDCLASS(RNWindowControl, Control);
	Window *window = nullptr;
	RNWindowContent *content = nullptr;
	RNPresentationCallbacks callbacks;
	ObjectID observed_root;
	Size2 declared_size;
	bool publishing = false;
	void _close_requested();
	void _resized();

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	RNWindowControl();
	Window *get_native_window() const { return window; }
	RNWindowContent *get_content() const { return content; }
	Size2 resolve_size(const Size2 &p_declared) const;
	void set_callbacks(RNPresentationCallbacks p_callbacks) { callbacks = std::move(p_callbacks); }
	void present(const String &p_title, const Size2 &p_size, const Size2 &p_declared_size, bool p_visible);
};
