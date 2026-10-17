#include "rn_text_input_component_view.h"

#include "rn_text_shaper.h"

#include <react/renderer/components/textinput/GodotTextInputProps.h>
#include <react/renderer/components/textinput/TextInputEventEmitter.h>

using namespace facebook::react;

namespace {

std::optional<::Color> optional_color(const SharedColor &p_color) {
	return p_color ? std::optional<::Color>(rn_godot_color(p_color)) : std::nullopt;
}

String submit_behavior(SubmitBehavior p_behavior) {
	switch (p_behavior) {
		case SubmitBehavior::Submit:
			return "submit";
		case SubmitBehavior::BlurAndSubmit:
			return "blurAndSubmit";
		case SubmitBehavior::Newline:
			return "newline";
		default:
			return String();
	}
}

} // namespace

RNTextInputComponentView::RNTextInputComponentView(Tag p_tag, std::shared_ptr<const RNTextShaper> p_shaper) :
		RNComponentView(p_tag, memnew(RNTextInputControl)), input(Object::cast_to<RNTextInputControl>(get_control())), shaper(std::move(p_shaper)) {
}

void RNTextInputComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto &next = static_cast<const GodotTextInputProps &>(*p_new);
	RNTextInputConfig config;
	config.font = shaper->resolve_font(next.textAttributes, config.font_size, config.font_color);
	config.selection_color = optional_color(next.selectionColor);
	config.cursor_color = optional_color(next.cursorColor);
	config.placeholder_color = optional_color(next.placeholderTextColor);
	config.editable = next.editable && !next.readOnly;
	config.placeholder = String::utf8(next.placeholder.c_str());
	config.secure = next.secureTextEntry;
	config.submit_behavior = submit_behavior(next.submitBehavior);
	config.multiline = next.multiline;
	config.max_length = next.maxLength == std::numeric_limits<int>::max() ? 0 : next.maxLength;
	config.auto_focus = next.autoFocus;
	config.targetable = next.pointerEvents != PointerEventsMode::None && next.pointerEvents != PointerEventsMode::BoxNone;
	input->configure(config);

	const auto previous = std::static_pointer_cast<const GodotTextInputProps>(p_old);
	const bool fresh_text = !previous || previous->text != next.text;
	const bool fresh_selection = next.selection && (!previous || previous->selection != next.selection);
	if (fresh_text || fresh_selection) {
		input->reconcile(next.mostRecentEventCount, fresh_text ? Variant(String::utf8(next.text.c_str())) : Variant(), fresh_selection ? next.selection->start : -1, fresh_selection ? next.selection->end : -1);
	}
}

void RNTextInputComponentView::_emit(const StringName &p_event, const Dictionary &p_payload) {
	auto emitter = std::static_pointer_cast<const TextInputEventEmitter>(event_emitter);
	if (!emitter) {
		return;
	}
	TextInputEventEmitter::Metrics metrics{};
	metrics.text = String(p_payload.get("text", String())).utf8().get_data();
	metrics.eventCount = p_payload.get("eventCount", 0);
	metrics.target = tag;
	const Dictionary selection = p_payload.get("selection", Dictionary());
	metrics.selectionRange = { .location = int(selection.get("start", 0)), .length = int(selection.get("end", 0)) - int(selection.get("start", 0)) };
	if (p_event == StringName("topChange")) {
		emitter->onChange(metrics);
	} else if (p_event == StringName("topSelectionChange")) {
		emitter->onSelectionChange(metrics);
	} else if (p_event == StringName("topSubmitEditing")) {
		emitter->onSubmitEditing(metrics);
	} else if (p_event == StringName("topEndEditing")) {
		emitter->onEndEditing(metrics);
	} else if (p_event == StringName("topFocus")) {
		emitter->onFocus(metrics);
	} else if (p_event == StringName("topBlur")) {
		emitter->onBlur(metrics);
	} else if (p_event == StringName("topKeyPress")) {
		emitter->onKeyPress({ .text = String(p_payload.get("key", String())).utf8().get_data(), .eventCount = metrics.eventCount });
	}
}

RNTextInputComponentView::~RNTextInputComponentView() {
	if (has_control()) {
		input->set_callbacks(nullptr, nullptr);
	}
}

void RNTextInputComponentView::update_event_emitter(const EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	input->set_callbacks([this](const StringName &p_event, const Dictionary &p_payload) { _emit(p_event, p_payload); }, nullptr);
}

void RNTextInputComponentView::handle_command(const std::string &p_name, const folly::dynamic &p_args) {
	if (p_name == "focus") {
		input->focus();
	} else if (p_name == "blur") {
		input->blur();
	} else if (p_name == "setTextAndSelection" && p_args.size() == 4) {
		input->reconcile(int(p_args[0].asInt()), p_args[1].isString() ? Variant(String::utf8(p_args[1].getString().c_str())) : Variant(), int(p_args[2].asInt()), int(p_args[3].asInt()));
	}
}
