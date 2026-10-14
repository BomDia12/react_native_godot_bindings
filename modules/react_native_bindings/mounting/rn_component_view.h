#pragma once

#include "scene/gui/control.h"
#include "scene/resources/style_box_flat.h"

#include <folly/dynamic.h>
#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/EventEmitter.h>
#include <react/renderer/core/LayoutMetrics.h>
#include <react/renderer/core/State.h>
#include <react/renderer/mounting/ShadowView.h>

#include <memory>

Variant rn_variant_from_dynamic(const folly::dynamic &p_value);
::Color rn_godot_color(const facebook::react::SharedColor &p_color, const ::Color &p_default = ::Color(0, 0, 0, 0));
Ref<StyleBoxFlat> rn_view_stylebox(const facebook::react::ViewProps &p_props, const facebook::react::LayoutMetrics &p_layout);

// The Godot side of one mounted React host. Subclasses own one Control and
// translate upstream props, state and layout into it on the main thread.
class RNComponentView {
	Control *control = nullptr;
	ObjectID control_id;

protected:
	facebook::react::Tag tag = 0;
	std::shared_ptr<const facebook::react::ViewProps> props;
	facebook::react::State::Shared state;
	facebook::react::EventEmitter::Shared event_emitter;
	facebook::react::LayoutMetrics layout_metrics;

	void apply_view_props(const facebook::react::ViewProps &p_props);
	void apply_transform();
	virtual Control *get_child_container() { return get_control(); }

public:
	RNComponentView(facebook::react::Tag p_tag, Control *p_control);
	virtual ~RNComponentView();
	RNComponentView(const RNComponentView &) = delete;
	RNComponentView &operator=(const RNComponentView &) = delete;

	facebook::react::Tag get_tag() const { return tag; }
	const facebook::react::EventEmitter::Shared &get_event_emitter() const { return event_emitter; }

	Control *get_control() const { return control; }
	virtual void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new);
	virtual void update_state(const facebook::react::State::Shared &p_state);
	virtual void update_layout(const facebook::react::LayoutMetrics &p_metrics);
	virtual void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter);
	virtual void insert_child(RNComponentView *p_child, int p_index);
	virtual void remove_child(RNComponentView *p_child);
	virtual void handle_command(const std::string &p_name, const folly::dynamic &p_args) {}
	virtual void synchronously_update_props(const folly::dynamic &p_props) {}
};

// The surface container a ReactNativeRootView hosts React children in.
class RNRootComponentView : public RNComponentView {
public:
	RNRootComponentView(facebook::react::Tag p_tag, Control *p_container) :
			RNComponentView(p_tag, p_container) {}
	void update_props(const facebook::react::Props::Shared &, const facebook::react::Props::Shared &) override {}
	void update_layout(const facebook::react::LayoutMetrics &) override {}
};
