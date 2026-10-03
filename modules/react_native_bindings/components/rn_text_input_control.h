#pragma once
#include "rn_font.h"

#include "scene/gui/line_edit.h"
#include "scene/gui/text_edit.h"

struct RNTextInputData : RNComponentData {
	RNFontSnapshot font;
};
class RNTextInputState : public RefCounted {
	GDCLASS(RNTextInputState, RefCounted);

protected:
	static void _bind_methods() {}

public:
	ObjectID editor_id;
	Control::MouseFilter wrapper_mouse_filter = Control::MOUSE_FILTER_PASS;
	Control::MouseFilter editor_mouse_filter = Control::MOUSE_FILTER_STOP;
	std::shared_ptr<const LineEdit::EditState> line;
	std::shared_ptr<const TextEdit::EditState> multiline;
	Dictionary props;
	Dictionary deferred;
	uint64_t text_revision = UINT64_MAX;
	uint64_t selection_revision = UINT64_MAX;
	int event_count = 0;
	String observed_text;
	Point2i observed_selection;
	bool pending_native_event = false;
};
class RNTextInputControl : public Control {
	GDCLASS(RNTextInputControl, Control);
	Control *editor = nullptr;
	Control *staged_editor = nullptr;
	Dictionary props;
	RNHostContext published_context;
	Dictionary deferred_replacement;
	bool changing = false;
	bool published = false;
	bool pending_native_event = false;
	int event_count = 0;
	uint64_t text_revision = UINT64_MAX;
	uint64_t selection_revision = UINT64_MAX;
	String observed_text;
	Point2i observed_selection;
	Ref<Font> subscribed_font;
	void _native_changed();
	void _line_changed(const String &p_text);
	void _selection_changed();
	void _focus_entered();
	void _focus_exited();
	void _submitted(const String &p_text);
	void _editor_input(const Ref<InputEvent> &p_event);
	void _font_changed();
	Control *_create_editor(bool p_multiline);
	void _configure_editor(Control *p_editor, const RNPreparedHostState &p_state);
	void _emit(const StringName &p_event, Dictionary p_payload = Dictionary());
	void _discard_notifications();
	void _set_text(const String &p_text);
	void _set_selection(int p_start, int p_end);

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	RNTextInputControl();
	~RNTextInputControl();
	Control *get_editor() const { return editor; }
	String get_text() const;
	Point2i get_selection() const;
	bool is_composing() const;
	int get_event_count() const { return event_count; }
	void apply(const RNPreparedHostState &p_state);
	void publish(const RNPreparedHostState &p_state, const RNHostContext &p_context);
	Ref<RNTextInputState> capture_state() const;
	void restore_state(const Ref<RNTextInputState> &p_state);
	void reconcile(int p_ack, const Variant &p_text, int p_start, int p_end);
	void focus();
	void blur();
};
std::shared_ptr<const RNHostDescriptor> rn_text_input_descriptor();
