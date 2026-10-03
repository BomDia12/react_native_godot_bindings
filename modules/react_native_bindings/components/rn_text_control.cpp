#include "rn_text_control.h"

#include "../fabric/rn_shadow_node.h"
#include "../fabric/rn_view_style.h"

#include "core/object/callable_mp.h"

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
		result[key] = p_child[key];
	}
	return result;
}

bool collect(const RNShadowNode &p_node, const Dictionary &p_inherited, RNTextDocument &r_document, RNError &r_error) {
	const Dictionary props = inherit(p_inherited, p_node.props);
	const int from = r_document.text.length();
	if (p_node.descriptor && p_node.descriptor->get_traits().contributes_text) {
		RNTextRun run;
		run.text = p_node.props.get("text", String());
		run.props = props;
		run.from = from;
		r_document.text += run.text;
		run.to = r_document.text.length();
		r_document.runs.push_back(run);
	} else if (p_node.view_name != "RCTText" && p_node.view_name != "RCTVirtualText") {
		r_error = RNError::make(RNErrorCode::VALIDATION, "Only text spans and raw text are supported inside Text", "text.prepare");
		return false;
	}
	for (const Ref<RNShadowNode> &child : p_node.children) {
		if (child.is_valid() && !collect(*child.ptr(), props, r_document, r_error)) {
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
		if (!collect(p_node, Dictionary(), *document, r_error)) {
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
		state->to = control->get_selection_to() + 1;
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
		RNTextControl *scratch = memnew(RNTextControl);
		scratch->set_external_layout_enabled(true);
		scratch->apply_document(document);
		const float width = p_constraints.width_mode == RNMeasureMode::UNDEFINED ? 1048576 : p_constraints.width;
		scratch->set_size(Size2(width, 1048576));
		scratch->validate_detached_layout();
		Size2 measured(scratch->get_content_width(), scratch->get_content_height());
		memdelete(scratch);
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
		RNTextControl *scratch = memnew(RNTextControl);
		scratch->set_external_layout_enabled(true);
		scratch->apply_document(document);
		scratch->set_size(p_size);
		const Array lines = scratch->get_text_layout_snapshot().get("lines", Array());
		const float result = lines.is_empty() ? p_size.y : float(Dictionary(lines[0])["baseline"]);
		memdelete(scratch);
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

RNTextControl::RNTextControl() {
	set_threaded(false);
	set_scroll_active(false);
	set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	set_clip_contents(true);
}

RNTextControl::~RNTextControl() {
	for (const Ref<Font> &font : observed_fonts) {
		font->disconnect("changed", callable_mp(this, &RNTextControl::_font_changed));
	}
}

void RNTextControl::_notification(int p_what) {
	if (p_what == NOTIFICATION_RESIZED && sink) {
		callable_mp(this, &RNTextControl::_publish_layout).call_deferred();
	}
}

void RNTextControl::_font_changed() {
	if (sink && sink->invalidate_layout) {
		sink->invalidate_layout();
	}
}

void RNTextControl::apply_document(const std::shared_ptr<const RNTextDocument> &p_document) {
	if (!p_document) {
		return;
	}
	if (document && document->revision == p_document->revision && document->text == p_document->text) {
		return;
	}
	int from = get_selection_from();
	int to = get_selection_to() + 1;
	if (document && from >= 0) {
		from = p_document->character_at(document->utf16_at(from));
		to = p_document->character_at(document->utf16_at(to));
	}
	document = p_document;
	clear();
	add_theme_font_override("normal_font", document->font.font);
	add_theme_font_size_override("normal_font_size", document->font.size);
	add_theme_color_override("default_color", document->font.color);
	add_theme_constant_override("line_separation", 0);
	add_theme_constant_override("paragraph_separation", 0);
	Ref<StyleBoxEmpty> empty;
	empty.instantiate();
	add_theme_style_override("normal", empty);
	add_theme_style_override("focus", empty);
	const String alignment = document->props.get("textAlign", "left");
	set_horizontal_alignment(alignment == "center" ? HORIZONTAL_ALIGNMENT_CENTER : alignment == "right" ? HORIZONTAL_ALIGNMENT_RIGHT
					: alignment == "justify"															? HORIZONTAL_ALIGNMENT_FILL
																										: HORIZONTAL_ALIGNMENT_LEFT);
	const String direction = document->props.get("writingDirection", document->props.get("direction", "auto"));
	set_text_direction(direction == "rtl" ? TEXT_DIRECTION_RTL : direction == "ltr" ? TEXT_DIRECTION_LTR
																					: TEXT_DIRECTION_AUTO);
	set_line_height_override(document->props.get("lineHeight", 0.0));
	const int line_limit = document->props.get("numberOfLines", 0);
	set_visible_line_limit(line_limit > 0 ? line_limit : -1);
	set_selection_enabled(document->props.get("selectable", false));
	for (const RNTextRun &run : document->runs) {
		push_font(run.font.font, run.font.size);
		push_color(run.font.color);
		const String decorations = run.props.get("textDecorationLine", "none");
		if (decorations.contains("underline")) {
			push_underline();
		}
		if (decorations.contains("line-through")) {
			push_strikethrough();
		}
		add_text(run.text);
		if (decorations.contains("line-through")) {
			pop();
		}
		if (decorations.contains("underline")) {
			pop();
		}
		pop();
		pop();
	}
	set_selection_range(from, to);
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
	const Dictionary layout = get_text_layout_snapshot();
	if (layout == published_layout) {
		return;
	}
	published_layout = layout.duplicate(true);
	Array lines;
	const Array native_lines = layout.get("lines", Array());
	for (const Dictionary line : native_lines) {
		const Rect2 bounds = line["bounds"];
		Dictionary record;
		record["x"] = bounds.position.x;
		record["y"] = bounds.position.y;
		record["width"] = bounds.size.x;
		record["height"] = bounds.size.y;
		record["ascender"] = real_t(line["baseline"]) - bounds.position.y;
		record["descender"] = bounds.size.y - real_t(record["ascender"]);
		const Ref<Font> metric_font = document->font.font;
		const int metric_size = document->font.size;
		const TypedArray<RID> font_rids = metric_font->get_rids();
		record["xHeight"] = font_rids.is_empty() ? 0.0 : TS->font_get_glyph_size(font_rids[0], Vector2i(metric_size, 0), TS->font_get_glyph_index(font_rids[0], metric_size, 'x', 0)).y;
		record["capHeight"] = font_rids.is_empty() ? 0.0 : TS->font_get_glyph_size(font_rids[0], Vector2i(metric_size, 0), TS->font_get_glyph_index(font_rids[0], metric_size, 'H', 0)).y;
		record["text"] = document->text.substr(int(line["from"]), int(line["to"]) - int(line["from"]));
		lines.push_back(record);
	}
	Dictionary payload;
	payload["lines"] = lines;
	if (sink->emit) {
		sink->emit(tag, "topTextLayout", payload, published_revision);
	}
}

HashMap<int, Vector<Rect2>> RNTextControl::get_span_bounds() {
	HashMap<int, Vector<Rect2>> result;
	if (!document) {
		return result;
	}
	Array ranges;
	Vector<int> tags;
	for (const KeyValue<int, Vector2i> &span : document->spans) {
		ranges.push_back(span.value);
		tags.push_back(span.key);
	}
	const Array bounds = get_text_layout_snapshot(ranges).get("ranges", Array());
	for (int i = 0; i < tags.size(); ++i) {
		const Array rectangles = bounds[i];
		for (const Rect2 rect : rectangles) {
			result[tags[i]].push_back(rect);
		}
	}
	return result;
}

std::shared_ptr<const RNHostDescriptor> rn_text_descriptor() {
	return std::make_shared<RNTextDescriptor>();
}
std::shared_ptr<const RNHostDescriptor> rn_virtual_text_descriptor() {
	return std::make_shared<RNVirtualTextDescriptor>();
}
