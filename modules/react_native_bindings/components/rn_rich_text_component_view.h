#pragma once

#include "../mounting/rn_component_view.h"

#include "scene/gui/rich_text_effect.h"
#include "scene/gui/rich_text_label.h"

#include <functional>
#include <string>
#include <vector>

// A host, because Godot's RichTextLabel can't be subclassed from a module. The
// label fills it.
class RNRichTextControl : public Control {
	GDCLASS(RNRichTextControl, Control);
	RichTextLabel *label = nullptr;
	std::function<void(const std::string &)> meta_clicked_callback;
	std::function<void(const std::string &)> meta_hover_started_callback;
	std::function<void(const std::string &)> meta_hover_ended_callback;
	std::function<void()> reveal_finished_callback;
	Ref<Tween> reveal_tween;

	void _meta_clicked(const Variant &p_meta);
	void _meta_hover_started(const Variant &p_meta);
	void _meta_hover_ended(const Variant &p_meta);
	void _reveal_finished();

protected:
	static void _bind_methods() {}

public:
	RNRichTextControl();
	RichTextLabel *get_label() const { return label; }
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	void set_callbacks(std::function<void(const std::string &)> p_clicked, std::function<void(const std::string &)> p_started, std::function<void(const std::string &)> p_ended, std::function<void()> p_finished);
	void reveal(double p_ratio, double p_seconds);
	void stop_reveal();
};

class RNRichTextComponentView : public RNComponentView {
	RNRichTextControl *host;
	RichTextLabel *label;
	std::vector<std::string> effect_paths;
	std::vector<uint64_t> effect_requests;
	Array effects;
	int effects_pending = 0;

	void _load_effects(const std::vector<std::string> &p_paths);
	void _cancel_effects();
	void _apply_text();

public:
	explicit RNRichTextComponentView(facebook::react::Tag p_tag);
	~RNRichTextComponentView() override;
	Vector<Control *> get_input_controls() const override { return { label }; }
	bool owns_input_control(Control *p_control) const override { return p_control == host || p_control == label; }
	Control *get_focus_control() const override { return label; }
	Node *get_native_node() const override { return label; }
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
	void handle_command(const std::string &p_name, const folly::dynamic &p_args) override;
};
