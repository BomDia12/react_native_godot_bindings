#pragma once

#include "rn_font.h"
#include "rn_paragraph_layout.h"

struct RNTextRun {
	String text;
	Dictionary props;
	RNFontSnapshot font;
	int from = 0;
	int to = 0;
	int tag = 0;
};

struct RNTextDocument : RNComponentData {
	String text;
	Vector<RNTextRun> runs;
	HashMap<int, Vector2i> spans;
	Vector<int> utf16_offsets;
	RNFontSnapshot font;
	Dictionary props;
	uint64_t revision = 0;
	int utf16_at(int p_character) const;
	int character_at(int p_utf16) const;
};

void rn_shape_text_document(const RNTextDocument &p_document, float p_width, RNParagraphLayout &r_layout);

class RNTextNativeState : public RefCounted {
	GDCLASS(RNTextNativeState, RefCounted);

protected:
	static void _bind_methods() {}

public:
	std::shared_ptr<const RNTextDocument> document;
	int from = -1;
	int to = -1;
	bool visible = true;
	Color modulate;
};

class RNTextControl : public Control {
	GDCLASS(RNTextControl, Control);

	std::shared_ptr<const RNTextDocument> document;
	std::shared_ptr<const RNHostEventSink> sink;
	RNParagraphLayout layout;
	Vector<Ref<Font>> observed_fonts;
	uint64_t published_revision = 0;
	int tag = 0;
	Dictionary published_layout;
	int selection_from = -1;
	int selection_to = -1;
	int selection_anchor = -1;
	bool selecting = false;
	void _font_changed();
	void _publish_layout();
	void _shape();

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void gui_input(const Ref<InputEvent> &p_event) override;

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	RNTextControl();
	~RNTextControl() override;
	String get_text() const;
	int get_font_size() const;
	Color get_text_color() const;
	void apply_document(const std::shared_ptr<const RNTextDocument> &p_document);
	std::shared_ptr<const RNTextDocument> get_document() const { return document; }
	const RNParagraphLayout &get_layout() const { return layout; }
	bool is_selectable() const;
	void set_selection_range(int p_from, int p_to);
	int get_selection_from() const { return selection_from; }
	int get_selection_to() const { return selection_to; }
	void publish(const RNHostContext &p_context);
	HashMap<int, Vector<Rect2>> get_span_bounds() const;
};

std::shared_ptr<const RNHostDescriptor> rn_text_descriptor();
std::shared_ptr<const RNHostDescriptor> rn_virtual_text_descriptor();
