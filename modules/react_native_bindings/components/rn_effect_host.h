#pragma once

#include "rn_filter.h"
#include "rn_view_control.h"

#include "scene/gui/subviewport_container.h"
#include "scene/main/viewport.h"

#include <react/renderer/graphics/BlendMode.h>

// Renders a view's subtree in its own SubViewport, so filters, blend modes and
// isolation apply to the subtree as a whole and nest freely. The subtree keeps
// behaving as if it were inline: Godot maps input and screen positions through
// the container, and the host passes keyboard focus in and out of it.
class RNEffectHost : public SubViewportContainer {
	GDCLASS(RNEffectHost, SubViewportContainer);

	SubViewport *viewport = nullptr;
	// The canvas group owner; a group skips its owner's own drawing, so the
	// view's painting lives in the painter below it.
	Control *content = nullptr;
	// With a filter, the painter renders in a viewport of its own inside the
	// group, because the renderer copies no backdrop while a group is open and
	// blend modes below the filter need one.
	SubViewportContainer *filtered = nullptr;
	SubViewport *filtered_viewport = nullptr;
	Ref<ShaderMaterial> filtered_material;
	RNViewControl *painter = nullptr;
	// Focus paths this host set on the first and last inner controls, and what
	// they held before, so they can be restored before new ones are set.
	ObjectID focus_first;
	ObjectID focus_last;
	NodePath saved_previous;
	NodePath saved_next;
	real_t margin = 0;
	Vector<RNFilterStage> stages;
	facebook::react::BlendMode blend = facebook::react::BlendMode::Normal;
	Ref<ShaderMaterial> filter_material;
	Ref<ShaderMaterial> blend_material;
	String filter_shader;

	void _update_blend();
	void _update_clip();
	void _enter_focus();
	void _set_focus_paths();
	void _restore_focus_paths();
	void _use_filtered_viewport(bool p_filtered);
	Control *_focusable(bool p_last) const;

protected:
	static void _bind_methods() {}
	void _notification(int p_what);
	void input(const Ref<InputEvent> &p_event) override;

public:
	RNEffectHost();
	Size2 get_combined_minimum_size() const override { return Size2(); }

	RNViewControl *get_painter() const { return painter; }
	// The viewport the painter and the view's children render in.
	SubViewport *get_effect_viewport() const { return filtered_viewport ? filtered_viewport : viewport; }
	// p_size is the view's size and p_margin how far its painting and filters reach past it.
	void configure(const Size2 &p_size, real_t p_margin, const Vector<RNFilterStage> &p_stages, facebook::react::BlendMode p_blend);
};
