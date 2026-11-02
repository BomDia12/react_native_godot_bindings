#pragma once

#include "../mounting/rn_component_view.h"

#include "scene/gui/texture_rect.h"

#include <functional>

class SubViewport;

// Shows a viewport GDScript registered under a name. The game keeps owning the
// viewport, which can show the live world, unlike a GodotScene.
class RNViewportViewControl : public TextureRect {
	GDCLASS(RNViewportViewControl, TextureRect);

	String viewport_name;
	ObjectID shown;
	bool match_size = false;
	bool forward_input = false;
	bool resolved = false;
	std::function<void()> loaded;
	std::function<void(const String &)> failed;

	SubViewport *_viewport() const;
	void _resolve();
	void _follow_viewport(SubViewport *p_viewport, bool p_follow);
	void _registration_changed(const String &p_name);
	void _viewport_exiting();
	void _update_viewport_size();
	Transform2D _to_viewport() const;

protected:
	static void _bind_methods() {}
	void _notification(int p_what);
	void gui_input(const Ref<InputEvent> &p_event) override;

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	void set_callbacks(std::function<void()> p_loaded, std::function<void(const String &)> p_failed);
	void set_viewport_name(const String &p_name);
	void set_match_size(bool p_match);
	void set_forward_input(bool p_forward);
};

class RNViewportViewComponentView : public RNComponentView {
	RNViewportViewControl *view;

public:
	explicit RNViewportViewComponentView(facebook::react::Tag p_tag);
	~RNViewportViewComponentView() override;
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
};
