#pragma once

#include "../components/rn_rounded_rect.h"

#include "scene/gui/control.h"

#include <folly/dynamic.h>
#include <react/renderer/components/view/TouchEventEmitter.h>
#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/ComponentDescriptor.h>
#include <react/renderer/core/EventEmitter.h>
#include <react/renderer/core/LayoutMetrics.h>
#include <react/renderer/core/State.h>
#include <react/renderer/mounting/ShadowView.h>

#include <functional>
#include <memory>

::Color rn_godot_color(const facebook::react::SharedColor &p_color, const ::Color &p_default = ::Color(0, 0, 0, 0));
// Applies each React Native control's cursor to p_subtree, inheriting from the
// nearest React Native ancestor where the cursor is auto, as CSS does.
void rn_refresh_cursors(Control *p_subtree);
RNRoundedRect rn_rounded_rect(const Rect2 &p_rect, const facebook::react::BorderRadii &p_radii);

struct RNTouchTarget {
	facebook::react::Tag tag = 0;
	std::shared_ptr<const facebook::react::TouchEventEmitter> emitter;
};

// The Godot side of one mounted React host. Subclasses own one Control and
// translate upstream props, state and layout into it on the main thread.
class RNComponentView {
	Control *control = nullptr;
	ObjectID control_id;

protected:
	facebook::react::Tag tag = 0;
	facebook::react::ComponentHandle component_handle = 0;
	bool owns_control = true;
	std::shared_ptr<const facebook::react::ViewProps> props;
	facebook::react::State::Shared state;
	facebook::react::EventEmitter::Shared event_emitter;
	facebook::react::LayoutMetrics layout_metrics;

	// Godot still reports focus, size and visibility changes while the control
	// leaves the tree, which this destructor does after a subclass is gone. A
	// subclass that gives its control callbacks into itself clears them in its
	// own destructor while has_control() holds.
	bool has_control() const { return ObjectDB::get_instance(control_id) != nullptr; }
	void apply_view_props(const facebook::react::ViewProps &p_props);
	void apply_transform();
	virtual Control *get_child_container() { return get_control(); }

public:
	static constexpr const char *TAG_META = "_react_native_tag";
	static constexpr const char *DETACHING_META = "_react_native_detaching";

	RNComponentView(facebook::react::Tag p_tag, Control *p_control);
	virtual ~RNComponentView();
	RNComponentView(const RNComponentView &) = delete;
	RNComponentView &operator=(const RNComponentView &) = delete;

	facebook::react::Tag get_tag() const { return tag; }
	facebook::react::ComponentHandle get_component_handle() const { return component_handle; }
	void set_component_handle(facebook::react::ComponentHandle p_handle) { component_handle = p_handle; }
	const facebook::react::EventEmitter::Shared &get_event_emitter() const { return event_emitter; }

	Control *get_control() const { return control; }
	virtual void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new);
	virtual void update_state(const facebook::react::State::Shared &p_state);
	virtual void update_layout(const facebook::react::LayoutMetrics &p_metrics);
	virtual void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter);
	virtual void insert_child(RNComponentView *p_child, int p_index);
	virtual void remove_child(RNComponentView *p_child);
	void finish_removal();
	virtual void handle_command(const std::string &p_name, const folly::dynamic &p_args) {}
	// The surface's theme metrics changed. Views that resolve fonts or colors
	// from them outside of layout apply them again.
	virtual void theme_changed() {}
	virtual void synchronously_update_props(const facebook::react::ComponentDescriptor &p_descriptor, const facebook::react::PropsParserContext &p_context, const folly::dynamic &p_props);

	// Controls besides get_control() whose gui_input belongs to this view.
	virtual Vector<Control *> get_input_controls() const { return {}; }
	virtual bool owns_input_control(Control *p_control) const { return p_control == control; }
	virtual Control *get_focus_control() const { return control; }
	// What ReactNativeRootView.get_native_node() returns for this view's nativeID.
	virtual Node *get_native_node() const { return control; }
	const std::string &get_native_id() const;
	// Views whose Godot control handles presses itself receive pointer events but no touches.
	virtual bool owns_activation() const { return false; }
	// Views that name other views as focus neighbors resolve them once each
	// mount transaction has placed every control.
	virtual bool has_focus_references() const { return false; }
	virtual void resolve_focus_references(const std::function<Control *(facebook::react::Tag p_tag)> &p_find_focus_control) {}
	virtual RNTouchTarget touch_target_at(const Point2 &p_local) const;
};

// The surface container a ReactNativeRootView hosts React children in. The
// root view keeps ownership of the container control.
class RNRootComponentView : public RNComponentView {
	std::function<void(const Size2 &)> size_listener;

public:
	RNRootComponentView(facebook::react::Tag p_tag, Control *p_container) :
			RNComponentView(p_tag, p_container) { owns_control = false; }
	// Reports the size React laid the surface out at, which differs from the
	// root's own size on the axes the root fits to its content.
	void set_size_listener(std::function<void(const Size2 &p_size)> p_listener) { size_listener = std::move(p_listener); }
	void update_props(const facebook::react::Props::Shared &, const facebook::react::Props::Shared &) override {}
	void update_layout(const facebook::react::LayoutMetrics &p_metrics) override {
		if (size_listener) {
			size_listener(Size2(p_metrics.frame.size.width, p_metrics.frame.size.height));
		}
	}
};
