#pragma once

#include "rn_font.h"

#include "scene/gui/rich_text_label.h"
#include "scene/resources/style_box.h"

struct RNTextRun {
	String text;
	Dictionary props;
	RNFontSnapshot font;
	int from = 0;
	int to = 0;
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

class RNTextControl : public RichTextLabel {
	GDCLASS(RNTextControl, RichTextLabel);

	std::shared_ptr<const RNTextDocument> document;
	std::shared_ptr<const RNHostEventSink> sink;
	Vector<Ref<Font>> observed_fonts;
	uint64_t published_revision = 0;
	int tag = 0;
	Dictionary published_layout;
	void _font_changed();
	void _publish_layout();

protected:
	static void _bind_methods() {}
	bool _set(const StringName &, const Variant &) { return false; }
	void _notification(int p_what);

public:
	RNTextControl();
	~RNTextControl() override;
	void apply_document(const std::shared_ptr<const RNTextDocument> &p_document);
	std::shared_ptr<const RNTextDocument> get_document() const { return document; }
	void publish(const RNHostContext &p_context);
	HashMap<int, Vector<Rect2>> get_span_bounds();
};

std::shared_ptr<const RNHostDescriptor> rn_text_descriptor();
std::shared_ptr<const RNHostDescriptor> rn_virtual_text_descriptor();
