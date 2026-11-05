#pragma once

#include "../mounting/rn_component_view.h"
#include "rn_clip.h"
#include "rn_effect_host.h"
#include "rn_view_control.h"

class RNViewComponentView : public RNComponentView {
	RNViewControl *view;
	RNEffectHost *effect = nullptr;
	RNClipControl *clip = nullptr;
	bool reported_drop_shadows = false;
	bool preferred_focus_pending = false;
	String skin_path;
	uint64_t skin_request = 0;
	Ref<StyleBox> skin;
	String material_path;
	uint64_t material_request = 0;
	Ref<ShaderMaterial> material;
	String drag_preview_path;
	uint64_t drag_preview_request = 0;
	Ref<Texture2D> drag_preview;
	void _load_drag_preview(const String &p_path);
	void _apply_drag();
	void _load_skin(const String &p_path);
	void _load_material(const String &p_path);
	void _apply_shader_parameters();
	void _apply_style();
	void _restructure(bool p_effect, bool p_clip);

protected:
	Control *get_child_container() override;

public:
	explicit RNViewComponentView(facebook::react::Tag p_tag);
	~RNViewComponentView() override;
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_layout(const facebook::react::LayoutMetrics &p_metrics) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
	void handle_command(const std::string &p_name, const folly::dynamic &p_args) override;
	bool has_focus_references() const override;
	void resolve_focus_references(const std::function<Control *(facebook::react::Tag p_tag)> &p_find_focus_control) override;
	RNEffectHost *get_effect_host() const { return effect; }
	RNClipControl *get_clip_host() const { return clip; }
};

RNAccessibility rn_view_accessibility(const facebook::react::ViewProps &p_props);
RNStyleBox::Paint rn_view_paint(const facebook::react::ViewProps &p_props, const facebook::react::LayoutMetrics &p_layout);
