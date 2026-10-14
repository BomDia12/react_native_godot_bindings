#include "rn_text_control.h"

#include "../fabric/rn_shadow_node.h"
#include "../fabric/rn_view_style.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/display/accessibility_server.h"
#include "servers/display/display_server.h"

#include <cmath>

namespace {

Dictionary text_config() {
	Dictionary event;
	event["registrationName"] = "onTextLayout";
	Dictionary direct;
	direct["topTextLayout"] = event;
	Dictionary config;
	config["directEventTypes"] = direct;
	return config;
}

Dictionary inherit(const Dictionary &p_parent, const Dictionary &p_child) {
	Dictionary result = p_parent.duplicate(true);
	for (const Variant &key : p_child.keys()) {
		if (String(key) == "maxFontSizeMultiplier" && p_child[key].get_type() == Variant::NIL) {
			continue;
		}
		result[key] = p_child[key];
	}
	return result;
}

bool collect(const RNShadowNode &p_node, const Dictionary &p_inherited, int p_span_tag, RNTextDocument &r_document, RNError &r_error) {
	const Dictionary props = inherit(p_inherited, p_node.props);
	const int from = r_document.text.length();
	const int span_tag = p_node.view_name == "RCTVirtualText" ? p_node.tag : p_span_tag;
	if (p_node.descriptor && p_node.descriptor->get_traits().contributes_text) {
		RNTextRun run;
		run.text = p_node.props.get("text", String());
		run.props = props;
		run.from = from;
		run.tag = span_tag;
		r_document.text += run.text;
		run.to = r_document.text.length();
		r_document.runs.push_back(run);
	} else if (p_node.view_name != "RCTText" && p_node.view_name != "RCTVirtualText") {
		r_error = RNError::make(RNErrorCode::VALIDATION, "Only text spans and raw text are supported inside Text", "text.prepare");
		return false;
	}
	for (const Ref<RNShadowNode> &child : p_node.children) {
		if (child.is_valid() && !collect(*child.ptr(), props, span_tag, r_document, r_error)) {
			return false;
		}
	}
	if (p_node.view_name == "RCTVirtualText") {
		r_document.spans[p_node.tag] = Vector2i(from, r_document.text.length());
	}
	r_document.revision = hash_murmur3_one_64(Variant(p_node.props).hash(), r_document.revision);
	return true;
}

class RNTextDescriptor : public RNHostDescriptor {
public:
	bool owns_native_activation(const RNPreparedHostState &p_state) const override { return p_state.props.get("selectable", false); }
	RNTextDescriptor() :
			RNHostDescriptor("RCTText", RNHostTraits{ true, true, false, true, false, true, false, true, false, true, true }, text_config()) {}

	bool prepare(const RNShadowNode &p_node, RNPreparedHostState &r_state, RNError &r_error) const override {
		if (!RNHostDescriptor::prepare(p_node, r_state, r_error)) {
			return false;
		}
		const Variant height_value = r_state.props.get("lineHeight", 0.0);
		const bool numeric_height = height_value.get_type() == Variant::INT || height_value.get_type() == Variant::FLOAT;
		const double line_height = numeric_height ? double(height_value) : -1;
		const Variant line_limit = r_state.props.get("numberOfLines", 0);
		const String ellipsis = r_state.props.get("ellipsizeMode", "tail");
		if (!std::isfinite(line_height) || line_height < 0 || line_height > 4096 || (line_limit.get_type() != Variant::INT && line_limit.get_type() != Variant::FLOAT) || !std::isfinite(double(line_limit)) || std::trunc(double(line_limit)) != double(line_limit) || double(line_limit) < 0 || double(line_limit) > 100000 || ellipsis != "tail") {
			r_error = RNError::make(RNErrorCode::VALIDATION, "Text requires bounded lineHeight, nonnegative numberOfLines and tail ellipsis", "text.prepare");
			return false;
		}
		auto document = std::make_shared<RNTextDocument>();
		document->props = r_state.props;
		if (!collect(p_node, Dictionary(), p_node.tag, *document, r_error)) {
			return false;
		}
		document->utf16_offsets.push_back(0);
		for (int i = 0; i < document->text.length(); ++i) {
			document->utf16_offsets.push_back(document->utf16_offsets[i] + (document->text[i] > 0xffff ? 2 : 1));
		}
		r_state.text = document->text;
		r_state.component_data = document;
		return true;
	}

	bool resolve_resources(RNPreparedHostState &r_state, const RNHostContext &p_context, RNError &r_error) const override {
		const auto prepared = std::static_pointer_cast<const RNTextDocument>(r_state.component_data);
		if (!prepared) {
			return false;
		}
		auto document = std::make_shared<RNTextDocument>();
		document->text = prepared->text;
		document->runs = prepared->runs;
		document->spans = prepared->spans;
		document->props = prepared->props;
		document->utf16_offsets = prepared->utf16_offsets;
		if (!rn_resolve_font(document->props, p_context, document->font, r_error)) {
			return false;
		}
		document->revision = hash_murmur3_one_64(Variant(document->props).hash(), document->font.revision);
		for (RNTextRun &run : document->runs) {
			if (!rn_resolve_font(run.props, p_context, run.font, r_error)) {
				return false;
			}
			document->revision = hash_murmur3_one_64(Variant(run.props).hash(), document->revision);
			document->revision = hash_murmur3_one_64(run.font.revision, document->revision);
			document->revision = hash_murmur3_one_64(run.text.hash(), document->revision);
		}
		r_state.dependency_revision = document->revision;
		r_state.component_data = document;
		return true;
	}

	Control *create_host(const RNHostContext &) const override { return memnew(RNTextControl); }

	bool apply(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &, RNError &) const override {
		RNTextControl *control = Object::cast_to<RNTextControl>(p_host);
		control->apply_document(std::static_pointer_cast<const RNTextDocument>(p_state.component_data));
		control->set_visible(String(p_state.props.get("display", "flex")) != "none");
		control->set_modulate(Color(1, 1, 1, RNViewStyle::opacity_of(p_state.props)));
		control->set_mouse_filter(p_state.branch_targetable ? Control::MOUSE_FILTER_PASS : Control::MOUSE_FILTER_IGNORE);
		return true;
	}

	Variant capture_state(Control *p_host) const override {
		RNTextControl *control = Object::cast_to<RNTextControl>(p_host);
		Ref<RNTextNativeState> state;
		state.instantiate();
		state->document = control->get_document();
		state->from = control->get_selection_from();
		state->to = control->get_selection_to();
		state->visible = control->is_visible();
		state->modulate = control->get_modulate();
		return state;
	}

	void restore_state(Control *p_host, const Variant &p_state) const override {
		const Ref<RNTextNativeState> state = p_state;
		RNTextControl *control = Object::cast_to<RNTextControl>(p_host);
		if (state.is_valid()) {
			control->apply_document(state->document);
			control->set_selection_range(state->from, state->to);
			control->set_visible(state->visible);
			control->set_modulate(state->modulate);
		}
	}

	Size2 measure(const RNPreparedHostState &p_state, const RNMeasureConstraints &p_constraints) const override {
		auto document = std::static_pointer_cast<const RNTextDocument>(p_state.component_data);
		if (document && document->font.font.is_null()) {
			RNPreparedHostState resolved = p_state;
			RNError error;
			if (!resolve_resources(resolved, RNHostContext(), error)) {
				return Size2();
			}
			document = std::static_pointer_cast<const RNTextDocument>(resolved.component_data);
		}
		if (!document || document->font.font.is_null()) {
			return Size2();
		}
		RNParagraphLayout layout;
		rn_shape_text_document(*document, p_constraints.width_mode == RNMeasureMode::UNDEFINED ? -1 : p_constraints.width, layout);
		Size2 measured = layout.get_size().ceil();
		if (p_constraints.width_mode == RNMeasureMode::EXACTLY) {
			measured.x = p_constraints.width;
		} else if (p_constraints.width_mode == RNMeasureMode::AT_MOST) {
			measured.x = MIN(measured.x, p_constraints.width);
		}
		if (p_constraints.height_mode == RNMeasureMode::EXACTLY) {
			measured.y = p_constraints.height;
		} else if (p_constraints.height_mode == RNMeasureMode::AT_MOST) {
			measured.y = MIN(measured.y, p_constraints.height);
		}
		return measured;
	}

	void after_publish(Control *p_host, const RNPreparedHostState &, const RNHostContext &p_context) const override {
		Object::cast_to<RNTextControl>(p_host)->publish(p_context);
	}

	float baseline(const RNPreparedHostState &p_state, const Size2 &p_size) const override {
		const auto document = std::static_pointer_cast<const RNTextDocument>(p_state.component_data);
		if (!document || document->font.font.is_null()) {
			return p_size.y;
		}
		RNParagraphLayout layout;
		rn_shape_text_document(*document, p_size.x, layout);
		const float result = layout.get_lines().is_empty() ? p_size.y : layout.get_lines()[0].baseline;
		return CLAMP(result, 0.0f, p_size.y);
	}

	RNHostGeometry read_geometry(Control *p_host, const RNHostContext &p_context) const override {
		RNHostGeometry geometry = RNHostDescriptor::read_geometry(p_host, p_context);
		geometry.span_bounds = Object::cast_to<RNTextControl>(p_host)->get_span_bounds();
		return geometry;
	}
};

class RNVirtualTextDescriptor : public RNHostDescriptor {
public:
	RNVirtualTextDescriptor() :
			RNHostDescriptor("RCTVirtualText", RNHostTraits{ false, false, false, false, false, true, false, true, false, false, true }) {}
};

} // namespace

int RNTextDocument::utf16_at(int p_character) const {
	return utf16_offsets.is_empty() ? 0 : utf16_offsets[CLAMP(p_character, 0, utf16_offsets.size() - 1)];
}

int RNTextDocument::character_at(int p_utf16) const {
	int character = 0;
	while (character + 1 < utf16_offsets.size() && utf16_offsets[character + 1] <= p_utf16) {
		++character;
	}
	return character;
}

void rn_shape_text_document(const RNTextDocument &p_document, float p_width, RNParagraphLayout &r_layout) {
	Vector<RNParagraphLayout::Fragment> fragments;
	for (const RNTextRun &run : p_document.runs) {
		RNParagraphLayout::Fragment fragment;
		fragment.text = run.text;
		fragment.font = run.font.font;
		fragment.font_size = run.font.size;
		fragment.color = run.font.color;
		fragment.line_height = double(run.props.get("lineHeight", 0.0)) * run.font.multiplier;
		const String decorations = run.props.get("textDecorationLine", "none");
		if (decorations.contains("underline")) {
			fragment.decorations |= RNParagraphLayout::DECORATION_UNDERLINE;
		}
		if (decorations.contains("line-through")) {
			fragment.decorations |= RNParagraphLayout::DECORATION_STRIKETHROUGH;
		}
		fragment.tag = run.tag;
		fragments.push_back(fragment);
	}
	RNParagraphLayout::Attributes attributes;
	attributes.width = p_width;
	attributes.max_lines = p_document.props.get("numberOfLines", 0);
	const String alignment = p_document.props.get("textAlign", "left");
	attributes.alignment = alignment == "center" ? HORIZONTAL_ALIGNMENT_CENTER : alignment == "right" ? HORIZONTAL_ALIGNMENT_RIGHT
			: alignment == "justify"																  ? HORIZONTAL_ALIGNMENT_FILL
																									  : HORIZONTAL_ALIGNMENT_LEFT;
	const String direction = p_document.props.get("writingDirection", p_document.props.get("direction", "auto"));
	attributes.direction = direction == "rtl" ? TextServer::DIRECTION_RTL : direction == "ltr" ? TextServer::DIRECTION_LTR
																							   : TextServer::DIRECTION_AUTO;
	r_layout.shape(fragments, attributes);
}

void RNTextControl::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_text"), &RNTextControl::get_text);
	ClassDB::bind_method(D_METHOD("get_font_size"), &RNTextControl::get_font_size);
	ClassDB::bind_method(D_METHOD("get_text_color"), &RNTextControl::get_text_color);
}

String RNTextControl::get_text() const {
	return text;
}

int RNTextControl::get_font_size() const {
	return document ? document->font.size : 0;
}

Color RNTextControl::get_text_color() const {
	return document ? document->font.color : Color();
}

RNTextControl::RNTextControl() {
	set_clip_contents(true);
	set_focus_mode(FOCUS_ACCESSIBILITY);
}

RNTextControl::~RNTextControl() {
	for (const Ref<Font> &font : observed_fonts) {
		font->disconnect("changed", callable_mp(this, &RNTextControl::_font_changed));
	}
}

void RNTextControl::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_RESIZED: {
			_shape();
			if (sink) {
				callable_mp(this, &RNTextControl::_publish_layout).call_deferred();
			}
		} break;
		case NOTIFICATION_DRAW: {
			if (selection_to > selection_from) {
				const Color color = has_selection_color ? selection_color : get_theme_color(SNAME("selection_color"), SNAME("RichTextLabel"));
				for (const Rect2 &rect : layout.get_range_rects(selection_from, selection_to)) {
					draw_rect(rect, color);
				}
			}
			layout.draw(get_canvas_item(), Point2());
		} break;
		case NOTIFICATION_ACCESSIBILITY_UPDATE: {
			const RID element = get_accessibility_element();
			ERR_FAIL_COND(element.is_null());
			AccessibilityServer::get_singleton()->update_set_role(element, AccessibilityServerEnums::AccessibilityRole::ROLE_STATIC_TEXT);
			AccessibilityServer::get_singleton()->update_set_value(element, text);
			AccessibilityServer::get_singleton()->update_set_text_align(element, accessibility_alignment);
		} break;
	}
}

void RNTextControl::gui_input(const Ref<InputEvent> &p_event) {
	if (!is_selectable()) {
		return;
	}
	Ref<InputEventMouseButton> button = p_event;
	if (button.is_valid() && button->get_button_index() == MouseButton::LEFT) {
		selecting = button->is_pressed();
		if (selecting) {
			selection_anchor = layout.hit_test(button->get_position());
			set_selection_range(-1, -1);
		}
		return;
	}
	Ref<InputEventMouseMotion> motion = p_event;
	if (motion.is_valid() && selecting) {
		const int position = layout.hit_test(motion->get_position());
		set_selection_range(MIN(selection_anchor, position), MAX(selection_anchor, position));
		return;
	}
	if (p_event->is_action_pressed(SNAME("ui_copy"), false, true) && selection_to > selection_from) {
		DisplayServer::get_singleton()->clipboard_set(text.substr(selection_from, selection_to - selection_from));
		accept_event();
	}
}

void RNTextControl::_shape() {
	if (shape_function) {
		shape_function(get_size().x, layout);
	}
	queue_redraw();
	queue_accessibility_update();
}

void RNTextControl::_font_changed() {
	if (sink && sink->invalidate_layout) {
		sink->invalidate_layout();
	}
}

bool RNTextControl::is_selectable() const {
	return selectable;
}

void RNTextControl::set_selection_range(int p_from, int p_to) {
	const int length = text.length();
	const bool valid = is_selectable() && p_from >= 0 && p_to > p_from;
	const int from = valid ? MIN(p_from, length) : -1;
	const int to = valid ? MIN(p_to, length) : -1;
	if (from == selection_from && to == selection_to) {
		return;
	}
	selection_from = from;
	selection_to = to;
	queue_redraw();
}

void RNTextControl::apply_document(const std::shared_ptr<const RNTextDocument> &p_document) {
	if (!p_document) {
		return;
	}
	if (document && document->revision == p_document->revision && document->text == p_document->text) {
		return;
	}
	int from = selection_from;
	int to = selection_to;
	if (document && from >= 0) {
		from = p_document->character_at(document->utf16_at(from));
		to = p_document->character_at(document->utf16_at(to));
	}
	document = p_document;
	const String alignment = p_document->props.get("textAlign", "left");
	accessibility_alignment = alignment == "center" ? HORIZONTAL_ALIGNMENT_CENTER : alignment == "right" ? HORIZONTAL_ALIGNMENT_RIGHT
			: alignment == "justify"																	 ? HORIZONTAL_ALIGNMENT_FILL
																										 : HORIZONTAL_ALIGNMENT_LEFT;
	Color color;
	has_selection_color = RNViewStyle::color_of(p_document->props, "selectionColor", color);
	selection_color = color;
	set_content(p_document->text, [p_document](float p_width, RNParagraphLayout &r_layout) { rn_shape_text_document(*p_document, p_width, r_layout); }, p_document->props.get("selectable", false));
	set_selection_range(from, to);
}

void RNTextControl::set_content(const String &p_text, RNTextShapeFunction p_shape_function, bool p_selectable) {
	if (p_text != text) {
		set_selection_range(-1, -1);
	}
	text = p_text;
	shape_function = std::move(p_shape_function);
	selectable = p_selectable;
	set_focus_mode(selectable ? FOCUS_ALL : FOCUS_ACCESSIBILITY);
	_shape();
}

void RNTextControl::set_selection_color(const Color &p_color) {
	selection_color = p_color;
	has_selection_color = true;
	queue_redraw();
}

void RNTextControl::publish(const RNHostContext &p_context) {
	tag = p_context.tag;
	sink = p_context.event_sink;
	published_revision = p_context.revision;
	for (const Ref<Font> &font : observed_fonts) {
		font->disconnect("changed", callable_mp(this, &RNTextControl::_font_changed));
	}
	observed_fonts.clear();
	if (document->font.base.is_valid()) {
		observed_fonts.push_back(document->font.base);
		document->font.base->connect("changed", callable_mp(this, &RNTextControl::_font_changed));
	}
	for (const RNTextRun &run : document->runs) {
		if (run.font.base.is_valid() && !observed_fonts.has(run.font.base)) {
			observed_fonts.push_back(run.font.base);
			run.font.base->connect("changed", callable_mp(this, &RNTextControl::_font_changed));
		}
	}
	_publish_layout();
}

void RNTextControl::_publish_layout() {
	if (!sink || !document || !is_inside_tree()) {
		return;
	}
	Array lines;
	const Ref<Font> metric_font = document->font.font;
	const int metric_size = document->font.size;
	const TypedArray<RID> font_rids = metric_font->get_rids();
	for (const RNParagraphLayout::Line &line : layout.get_lines()) {
		Dictionary record;
		record["x"] = line.bounds.position.x;
		record["y"] = line.bounds.position.y;
		record["width"] = line.bounds.size.x;
		record["height"] = line.bounds.size.y;
		record["ascender"] = line.baseline - line.bounds.position.y;
		record["descender"] = line.bounds.size.y - real_t(record["ascender"]);
		record["xHeight"] = font_rids.is_empty() ? 0.0 : TS->font_get_glyph_size(font_rids[0], Vector2i(metric_size, 0), TS->font_get_glyph_index(font_rids[0], metric_size, 'x', 0)).y;
		record["capHeight"] = font_rids.is_empty() ? 0.0 : TS->font_get_glyph_size(font_rids[0], Vector2i(metric_size, 0), TS->font_get_glyph_index(font_rids[0], metric_size, 'H', 0)).y;
		record["text"] = document->text.substr(line.range.x, line.range.y - line.range.x);
		lines.push_back(record);
	}
	Dictionary payload;
	payload["lines"] = lines;
	if (payload == published_layout) {
		return;
	}
	published_layout = payload;
	if (sink->emit) {
		sink->emit(tag, "topTextLayout", payload, published_revision);
	}
}

HashMap<int, Vector<Rect2>> RNTextControl::get_span_bounds() const {
	HashMap<int, Vector<Rect2>> result;
	if (!document) {
		return result;
	}
	for (const KeyValue<int, Vector2i> &span : document->spans) {
		result[span.key] = layout.get_range_rects(span.value.x, span.value.y);
	}
	return result;
}

std::shared_ptr<const RNHostDescriptor> rn_text_descriptor() {
	return std::make_shared<RNTextDescriptor>();
}
std::shared_ptr<const RNHostDescriptor> rn_virtual_text_descriptor() {
	return std::make_shared<RNVirtualTextDescriptor>();
}
