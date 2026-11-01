#include "rn_rich_text_component_view.h"

#include "rn_resource_requests.h"

#include "core/object/callable_mp.h"
#include "scene/animation/tween.h"

#include <react/renderer/components/ReactNativeGodotSpec/EventEmitters.h>
#include <react/renderer/components/ReactNativeGodotSpec/Props.h>

using namespace facebook::react;

namespace {

std::string meta_string(const Variant &p_meta) {
	return String(p_meta).utf8().get_data();
}

} // namespace

RNRichTextControl::RNRichTextControl() {
	label = memnew(RichTextLabel);
	label->set_use_bbcode(true);
	label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	label->set_mouse_filter(MOUSE_FILTER_PASS);
	add_child(label, false, INTERNAL_MODE_FRONT);
	label->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	label->connect("meta_clicked", callable_mp(this, &RNRichTextControl::_meta_clicked));
	label->connect("meta_hover_started", callable_mp(this, &RNRichTextControl::_meta_hover_started));
	label->connect("meta_hover_ended", callable_mp(this, &RNRichTextControl::_meta_hover_ended));
}

void RNRichTextControl::set_callbacks(std::function<void(const std::string &)> p_clicked, std::function<void(const std::string &)> p_started, std::function<void(const std::string &)> p_ended, std::function<void()> p_finished) {
	meta_clicked_callback = std::move(p_clicked);
	meta_hover_started_callback = std::move(p_started);
	meta_hover_ended_callback = std::move(p_ended);
	reveal_finished_callback = std::move(p_finished);
}

void RNRichTextControl::_meta_clicked(const Variant &p_meta) {
	if (meta_clicked_callback) {
		meta_clicked_callback(meta_string(p_meta));
	}
}

void RNRichTextControl::_meta_hover_started(const Variant &p_meta) {
	if (meta_hover_started_callback) {
		meta_hover_started_callback(meta_string(p_meta));
	}
}

void RNRichTextControl::_meta_hover_ended(const Variant &p_meta) {
	if (meta_hover_ended_callback) {
		meta_hover_ended_callback(meta_string(p_meta));
	}
}

void RNRichTextControl::_reveal_finished() {
	reveal_tween.unref();
	if (reveal_finished_callback) {
		reveal_finished_callback();
	}
}

void RNRichTextControl::stop_reveal() {
	if (reveal_tween.is_valid()) {
		reveal_tween->kill();
		reveal_tween.unref();
	}
}

void RNRichTextControl::reveal(double p_ratio, double p_seconds) {
	stop_reveal();
	reveal_tween = create_tween();
	reveal_tween->tween_property(label, NodePath("visible_ratio"), p_ratio, p_seconds);
	reveal_tween->connect(SceneStringName(finished), callable_mp(this, &RNRichTextControl::_reveal_finished));
}

RNRichTextComponentView::RNRichTextComponentView(Tag p_tag) :
		RNComponentView(p_tag, memnew(RNRichTextControl)), host(Object::cast_to<RNRichTextControl>(get_control())), label(host->get_label()) {
}

RNRichTextComponentView::~RNRichTextComponentView() {
	_cancel_effects();
	if (has_control()) {
		host->set_callbacks(nullptr, nullptr, nullptr, nullptr);
	}
}

void RNRichTextComponentView::_cancel_effects() {
	for (uint64_t request : effect_requests) {
		RNResourceRequests::cancel(request);
	}
	effect_requests.clear();
}

void RNRichTextComponentView::_load_effects(const std::vector<std::string> &p_paths) {
	if (p_paths == effect_paths) {
		return;
	}
	effect_paths = p_paths;
	_cancel_effects();
	effects.clear();
	effects_pending = int(p_paths.size());
	if (effects_pending == 0) {
		label->set_effects(effects);
		return;
	}
	effects.resize(effects_pending);
	for (size_t i = 0; i < p_paths.size(); ++i) {
		const String path = String::utf8(p_paths[i].c_str());
		const uint64_t request = RNResourceRequests::request(path, "RichTextEffect", [this, i, path](const Ref<Resource> &p_resource, const String &p_error) {
			effects[int(i)] = p_resource;
			if (Object::cast_to<RichTextEffect>(*p_resource) == nullptr) {
				ERR_PRINT(vformat("E_RESOURCE: GodotRichText on React tag %d: %s", get_tag(), p_error.is_empty() ? vformat("'%s' is not a RichTextEffect.", path) : p_error));
			}
			if (--effects_pending == 0) {
				_apply_text();
			}
		});
		effect_requests.push_back(request);
	}
}

// The parser only knows the tags of installed effects, so text waits for them.
void RNRichTextComponentView::_apply_text() {
	if (effects_pending > 0 || !props) {
		return;
	}
	Array installed;
	for (int i = 0; i < effects.size(); ++i) {
		if (Object::cast_to<RichTextEffect>(effects[i])) {
			installed.push_back(effects[i]);
		}
	}
	label->set_effects(installed);
	label->set_text(String::utf8(static_cast<const GodotRichTextProps &>(*props).bbcode.c_str()));
}

void RNRichTextComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto old = std::static_pointer_cast<const GodotRichTextProps>(p_old);
	const auto &next = static_cast<const GodotRichTextProps &>(*p_new);
	label->set_theme_type_variation(String::utf8(next.godotThemeType.c_str()));
	label->set_scroll_active(next.scrollActive);
	label->set_selection_enabled(next.selectionEnabled);
	label->set_visible_characters_behavior(TextServer::VisibleCharactersBehavior(int(next.visibleCharactersBehavior)));
	const bool effects_changed = next.customEffects != effect_paths;
	_load_effects(next.customEffects);
	if (effects_changed || !old || old->bbcode != next.bbcode) {
		_apply_text();
	}
	// Only a changed prop stops a reveal, so rerenders leave it running.
	if (!old || old->visibleRatio != next.visibleRatio) {
		host->stop_reveal();
		label->set_visible_ratio(next.visibleRatio);
	}
}

void RNRichTextComponentView::update_event_emitter(const EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	auto emitter = std::static_pointer_cast<const GodotRichTextEventEmitter>(p_emitter);
	if (!emitter) {
		host->set_callbacks(nullptr, nullptr, nullptr, nullptr);
		return;
	}
	host->set_callbacks(
			[emitter](const std::string &p_meta) { emitter->onMetaClick({ .meta = p_meta }); },
			[emitter](const std::string &p_meta) { emitter->onMetaHoverStarted({ .meta = p_meta }); },
			[emitter](const std::string &p_meta) { emitter->onMetaHoverEnded({ .meta = p_meta }); },
			[emitter]() { emitter->onRevealFinished({}); });
}

void RNRichTextComponentView::handle_command(const std::string &p_name, const folly::dynamic &p_args) {
	if (p_name == "reveal" && p_args.size() == 2) {
		host->reveal(p_args[0].asDouble(), p_args[1].asDouble());
	}
}
