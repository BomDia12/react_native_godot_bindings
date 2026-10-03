#pragma once
#include "../components/rn_host_descriptor_registry.h"
#include "../components/rn_image_control.h"
#include "../components/rn_presentation_control.h"
#include "../components/rn_scroll_control.h"
#include "../components/rn_small_controls.h"
#include "../components/rn_text_input_control.h"
#include "../fabric/rn_shadow_node.h"
#include "../mounting/rn_layout_tree.h"
#include "../runtime/react_native_runtime_coordinator.h"

#include "core/crypto/crypto_core.h"
#include "core/object/message_queue.h"
#include "scene/main/scene_tree.h"
#include "tests/display_server_mock.h"
#include "tests/test_macros.h"

namespace TestRNComponents {
std::shared_ptr<const RNHostDescriptor> descriptor(const StringName &p_name) {
	return ReactNativeRuntimeCoordinator::get_singleton()->get_descriptor_registry()->find(p_name);
}
RNPreparedHostState prepare(const StringName &p_name, const Dictionary &p_props) {
	RNShadowNode node;
	node.view_name = p_name;
	node.tag = 42;
	node.props = p_props;
	node.descriptor = descriptor(p_name);
	RNPreparedHostState state;
	RNError error;
	REQUIRE(bool(node.descriptor));
	REQUIRE(node.descriptor->prepare(node, state, error));
	REQUIRE(node.descriptor->resolve_resources(state, RNHostContext(), error));
	return state;
}
Dictionary source(const String &p_uri, bool p_stateless = false) {
	Dictionary result;
	result["uri"] = p_uri;
	if (p_stateless) {
		result["credentials"] = "omit";
	}
	return result;
}
Vector<uint8_t> png_bytes() {
	Ref<Image> image = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 0, 0));
	return image->save_png_to_buffer();
}
String data_uri() {
	const auto bytes = png_bytes();
	return "data:image/png;base64," + CryptoCore::b64_encode_str(bytes.ptr(), bytes.size());
}
class Transport : public RNImageTransport {
public:
	struct Work {
		RNImageSource source;
		std::function<void(RNImageTransportResponse)> completion;
	};
	std::map<uint64_t, Work> work;
	uint64_t next = 0;
	int cancellations = 0;
	uint64_t start(const RNImageSource &p_source, uint64_t, std::function<void(RNImageTransportResponse)> p_completion) override {
		work[++next] = { p_source, p_completion };
		return next;
	}
	bool cancel(uint64_t) override {
		++cancellations;
		return false;
	}
	void finish(uint64_t p_id, bool p_cacheable = true) {
		auto completion = std::move(work[p_id].completion);
		work.erase(p_id);
		RNImageTransportResponse result;
		result.bytes = png_bytes();
		result.cacheable = p_cacheable;
		completion(std::move(result));
	}
};
TEST_CASE("[ReactNativeBindings][Images] codec header bounds and reservation lifetime precede allocation") {
	const auto bytes = png_bytes();
	Image::LoadLimits limits;
	limits.max_allocation_bytes = 4;
	limits.max_encoded_bytes = bytes.size();
	limits.max_workspace_bytes = 32 * 1024 * 1024;
	Size2i size;
	Image::BufferFormat format;
	{
		Image::ScopedLoadLimits scope(limits);
		CHECK(Image::probe_buffer(bytes.ptr(), bytes.size(), size, format) == ERR_OUT_OF_MEMORY);
		CHECK_FALSE(Image::is_load_size_allowed(UINT64_MAX, UINT64_MAX, 4));
		Image::LoadLimits tighter = limits;
		tighter.max_encoded_bytes = 1;
		{
			Image::ScopedLoadLimits nested(tighter);
			CHECK_FALSE(Image::is_load_buffer_allowed(2));
		}
		CHECK(Image::is_load_buffer_allowed(2));
	}
	CHECK(Image::get_load_limits().max_allocation_bytes == 0);
	RNImageLimits config;
	auto service = std::make_shared<RNImageService>(config);
	RNImageSource normalized;
	RNError error;
	REQUIRE(RNImageService::normalize(source(data_uri()), normalized, error));
	RNImageResult result;
	service->request(normalized, [&result](RNImageResult loaded) { result = loaded; });
	REQUIRE_FALSE(result.error.is_set());
	REQUIRE(bool(result.resource));
	CHECK(result.resource->pixels == Size2i(2, 2));
	CHECK(service->used_bytes() == 1152);
	auto retained = result.resource;
	result.resource.reset();
	service->clear_cache();
	CHECK(service->used_bytes() == 1152);
	retained.reset();
	CHECK(service->used_bytes() == 0);
}
TEST_CASE("[ReactNativeBindings][Images] credentialed sources bypass cache and dedup while canceled transport retains reservations") {
	RNImageLimits config;
	config.total_bytes = 128 * 1024 * 1024;
	auto service = std::make_shared<RNImageService>(config);
	auto transport = std::make_shared<Transport>();
	service->set_transport(transport);
	RNImageSource stateless;
	RNImageSource credentialed;
	RNError error;
	REQUIRE(RNImageService::normalize(source("https://fixture.invalid/icon.png", true), stateless, error));
	REQUIRE(RNImageService::normalize(source("https://fixture.invalid/icon.png"), credentialed, error));
	CHECK(stateless.shared);
	CHECK_FALSE(credentialed.shared);
	Dictionary authorization = source(stateless.uri, true);
	Dictionary headers;
	headers["aUtHoRiZaTiOn"] = "opaque";
	authorization["headers"] = headers;
	RNImageSource authorized;
	REQUIRE(RNImageService::normalize(authorization, authorized, error));
	CHECK_FALSE(authorized.shared);
	int completions = 0;
	auto first = service->request(stateless, [&completions](RNImageResult result) { CHECK_FALSE(result.error.is_set()); ++completions; });
	auto second = service->request(stateless, [&completions](RNImageResult result) { CHECK_FALSE(result.error.is_set()); ++completions; });
	CHECK(transport->next == 1);
	CHECK(service->used_bytes() == 56 * 1024 * 1024);
	service->cancel(first);
	CHECK(transport->cancellations == 0);
	service->cancel(second);
	CHECK(transport->cancellations == 1);
	CHECK(service->used_bytes() == 56 * 1024 * 1024);
	transport->finish(1);
	CHECK(completions == 0);
	CHECK(service->used_bytes() == 0);
	service->request(credentialed, [&completions](RNImageResult result) { CHECK_FALSE(result.error.is_set()); ++completions; });
	service->request(credentialed, [&completions](RNImageResult result) { CHECK_FALSE(result.error.is_set()); ++completions; });
	CHECK(transport->next == 3);
	int rejected = 0;
	service->request(credentialed, [&rejected](RNImageResult result) { CHECK(result.error.code == RNErrorCode::LIMIT); ++rejected; });
	CHECK(rejected == 1);
	transport->finish(2);
	transport->finish(3);
	CHECK(completions == 2);
	CHECK_FALSE(bool(service->cached(credentialed)));
	CHECK(service->used_bytes() == 0);
	service->request(stateless, [](RNImageResult result) { CHECK_FALSE(result.error.is_set()); });
	transport->finish(4);
	CHECK(bool(service->cached(stateless)));
	CHECK_FALSE(bool(service->cached(credentialed)));
	service->clear_cache();
	service->request(stateless, [](RNImageResult result) { CHECK_FALSE(result.error.is_set()); });
	transport->finish(5, false);
	CHECK_FALSE(bool(service->cached(stateless)));
	CHECK(service->used_bytes() == 0);
}
TEST_CASE("[ReactNativeBindings][TextInput][SceneTree] native edit checkpoints preserve undo and redo without restoring through setters") {
	LineEdit *line = memnew(LineEdit);
	SceneTree::get_singleton()->get_root()->add_child(line);
	line->set_text("start");
	line->set_caret_column(5);
	line->insert_text_at_caret(" one");
	line->delete_text(0, 0);
	MessageQueue::get_singleton()->flush();
	line->insert_text_at_caret(" two");
	line->delete_text(0, 0);
	MessageQueue::get_singleton()->flush();
	line->undo();
	const String retained = line->get_text();
	auto checkpoint = line->capture_edit_state();
	line->set_text("discarded");
	line->restore_edit_state(checkpoint);
	CHECK(line->get_text() == retained);
	line->redo();
	CHECK(line->get_text() == "start one two");
	line->undo();
	CHECK(line->get_text() == retained);
	memdelete(line);
	TextEdit *edit = memnew(TextEdit);
	edit->set_text("start");
	edit->set_caret_column(5);
	edit->insert_text_at_caret(" one");
	edit->begin_complex_operation();
	edit->insert_text_at_caret(" two");
	edit->end_complex_operation();
	edit->undo();
	auto multi = edit->capture_edit_state();
	const String old = edit->get_text();
	edit->set_text("discarded");
	edit->restore_edit_state(multi);
	CHECK(edit->get_text() == old);
	CHECK(edit->has_redo());
	edit->redo();
	CHECK(edit->get_text().contains("two"));
	edit->undo();
	CHECK(edit->get_text() == old);
	memdelete(edit);
}
TEST_CASE("[ReactNativeBindings][TextInput][SceneTree] acknowledgments use UTF16 and mode staging rolls back the retained editor") {
	Dictionary props;
	props["text"] = String::utf8("A😀z");
	props["fontSize"] = 16;
	props["mostRecentEventCount"] = 0;
	auto state = prepare("GodotTextInput", props);
	state.declarative_prop_revisions["text"] = 1;
	RNTextInputControl *control = memnew(RNTextInputControl);
	control->apply(state);
	CHECK(control->get_text() == String::utf8("A😀z"));
	control->reconcile(0, Variant(), 1, 3);
	CHECK(control->get_selection() == Point2i(1, 3));
	const auto checkpoint = control->capture_state();
	const auto editor_id = control->get_editor()->get_instance_id();
	auto *line = Object::cast_to<LineEdit>(control->get_editor());
	line->set_text("native");
	line->emit_signal("text_changed", "native");
	CHECK(control->get_event_count() == 1);
	control->reconcile(0, "stale", 0, 0);
	CHECK(control->get_text() == "native");
	control->reconcile(1, "accepted", 0, 0);
	CHECK(control->get_text() == "accepted");
	props["multiline"] = true;
	auto staged = prepare("GodotTextInput", props);
	control->apply(staged);
	CHECK(control->get_editor()->get_instance_id() == editor_id);
	control->restore_state(checkpoint);
	CHECK(control->get_editor()->get_instance_id() == editor_id);
	CHECK(control->get_text() == String::utf8("A😀z"));
	CHECK(control->get_selection() == Point2i(1, 3));
	CHECK(control->get_event_count() == 0);
	control->apply(staged);
	control->publish(staged, RNHostContext());
	CHECK(Object::cast_to<TextEdit>(control->get_editor()) != nullptr);
	CHECK(control->get_editor()->get_instance_id() != editor_id);
	memdelete(control);
}
TEST_CASE("[ReactNativeBindings][SmallControls][SceneTree] controlled switch restores live state and publishes one native change") {
	Dictionary props;
	props["value"] = false;
	auto state = prepare("RCTSwitch", props);
	auto host_descriptor = descriptor("RCTSwitch");
	auto *control = Object::cast_to<RNSwitchControl>(host_descriptor->create_host(RNHostContext()));
	RNError error;
	REQUIRE(host_descriptor->apply(control, state, RNHostContext(), error));
	int events = 0;
	auto sink = std::make_shared<RNHostEventSink>();
	sink->emit = [&events](int, const StringName &name, const Dictionary &payload, uint64_t) { CHECK(name == "topChange"); CHECK(bool(payload["value"])); ++events; };
	RNHostContext context;
	context.event_sink = sink;
	host_descriptor->after_publish(control, state, context);
	control->set_pressed(true);
	CHECK(events == 1);
	const auto captured = host_descriptor->capture_state(control);
	REQUIRE(host_descriptor->apply(control, state, context, error));
	CHECK(events == 1);
	CHECK_FALSE(control->is_pressed());
	host_descriptor->restore_state(control, captured);
	CHECK(control->is_pressed());
	CHECK(events == 1);
	memdelete(control);
}
TEST_CASE("[ReactNativeBindings][SmallControls][SceneTree] native switch pointer eligibility preserves enabled visuals and restores on rollback") {
	Dictionary props;
	auto state = prepare("RCTSwitch", props);
	auto host = descriptor("RCTSwitch");
	auto *control = Object::cast_to<RNSwitchControl>(host->create_host(RNHostContext()));
	RNError error;
	REQUIRE(host->apply(control, state, RNHostContext(), error));
	const Variant original = host->capture_state(control);
	state.branch_targetable = false;
	REQUIRE(host->apply(control, state, RNHostContext(), error));
	CHECK(control->get_mouse_filter() == Control::MOUSE_FILTER_IGNORE);
	CHECK_FALSE(control->is_disabled());
	host->restore_state(control, original);
	CHECK(control->get_mouse_filter() == Control::MOUSE_FILTER_STOP);
	for (const char *mode : { "none", "box-none", "box-only", "auto" }) {
		props["pointerEvents"] = mode;
		REQUIRE(host->apply(control, prepare("RCTSwitch", props), RNHostContext(), error));
		CHECK((control->get_mouse_filter() == Control::MOUSE_FILTER_IGNORE) == (String(mode) == "none" || String(mode) == "box-none"));
		CHECK_FALSE(control->is_disabled());
	}
	memdelete(control);
}

TEST_CASE("[ReactNativeBindings][TextInput][SceneTree] pointer rejection covers active and staged native editors and survives rollback") {
	auto *control = memnew(RNTextInputControl);
	SceneTree::get_singleton()->get_root()->add_child(control);
	Dictionary props;
	props["text"] = "Retained";
	auto state = prepare("GodotTextInput", props);
	control->apply(state);
	control->publish(state, RNHostContext());
	const auto original = control->capture_state();
	state.branch_targetable = false;
	control->apply(state);
	CHECK(control->get_mouse_filter() == Control::MOUSE_FILTER_IGNORE);
	CHECK(control->get_editor()->get_mouse_filter() == Control::MOUSE_FILTER_IGNORE);
	CHECK(Object::cast_to<LineEdit>(control->get_editor())->is_editable());
	props["multiline"] = true;
	auto staged = prepare("GodotTextInput", props);
	staged.branch_targetable = false;
	control->apply(staged);
	CHECK(control->get_editor()->get_mouse_filter() == Control::MOUSE_FILTER_IGNORE);
	CHECK(Object::cast_to<Control>(control->get_child(1))->get_mouse_filter() == Control::MOUSE_FILTER_IGNORE);
	control->restore_state(original);
	CHECK(control->get_child_count() == 1);
	CHECK(control->get_mouse_filter() == Control::MOUSE_FILTER_PASS);
	CHECK(control->get_editor()->get_mouse_filter() == Control::MOUSE_FILTER_STOP);
	control->apply(staged);
	control->publish(staged, RNHostContext());
	CHECK(Object::cast_to<TextEdit>(control->get_editor())->is_editable());
	CHECK(control->get_editor()->get_mouse_filter() == Control::MOUSE_FILTER_IGNORE);
	CHECK(control->get_text() == "Retained");
	for (const char *mode : { "none", "box-none", "auto" }) {
		props["pointerEvents"] = mode;
		control->apply(prepare("GodotTextInput", props));
		CHECK((control->get_editor()->get_mouse_filter() == Control::MOUSE_FILTER_IGNORE) == (String(mode) != "auto"));
	}
	memdelete(control);
}

TEST_CASE("[ReactNativeBindings][TextInput][SceneTree] autofocus runs once after initial publication and preserves later native focus") {
	auto *control = memnew(RNTextInputControl);
	auto *peer = memnew(LineEdit);
	SceneTree::get_singleton()->get_root()->add_child(control);
	SceneTree::get_singleton()->get_root()->add_child(peer);
	Dictionary props;
	props["autoFocus"] = true;
	auto state = prepare("GodotTextInput", props);
	peer->grab_focus();
	control->apply(state);
	CHECK(peer->has_focus());
	control->publish(state, RNHostContext());
	CHECK(control->get_editor()->has_focus());
	peer->grab_focus();
	control->apply(state);
	control->publish(state, RNHostContext());
	CHECK(peer->has_focus());
	props["multiline"] = true;
	state = prepare("GodotTextInput", props);
	control->apply(state);
	control->publish(state, RNHostContext());
	CHECK(peer->has_focus());
	memdelete(control);
	memdelete(peer);
}

TEST_CASE("[ReactNativeBindings][SmallControls][SceneTree] indicator opacity composes with tint alpha and restores on rollback") {
	Dictionary props;
	props["color"] = Color(0.2, 0.4, 0.6, 0.5);
	props["opacity"] = 0.3;
	auto host = descriptor("RCTActivityIndicatorView");
	auto *control = host->create_host(RNHostContext());
	RNError error;
	REQUIRE(host->apply(control, prepare("RCTActivityIndicatorView", props), RNHostContext(), error));
	CHECK(control->get_modulate().r == doctest::Approx(0.2));
	CHECK(control->get_modulate().a == doctest::Approx(0.15));
	const auto original = host->capture_state(control);
	props["opacity"] = 0.0;
	REQUIRE(host->apply(control, prepare("RCTActivityIndicatorView", props), RNHostContext(), error));
	CHECK(control->get_modulate().a == 0.0);
	host->restore_state(control, original);
	CHECK(control->get_modulate().a == doctest::Approx(0.15));
	memdelete(control);
}

TEST_CASE("[ReactNativeBindings][ScrollView][SceneTree] native wheel honors local pointer rejection and rollback restores input") {
	Window *viewport = SceneTree::get_singleton()->get_root();
	viewport->set_size(Size2i(640, 480));
	auto *scroll = memnew(RNScrollControl);
	viewport->add_child(scroll);
	scroll->set_external_layout_enabled(true);
	scroll->set_position(Point2(10, 10));
	scroll->set_size(Size2(240, 100));
	auto *rows = memnew(Control);
	rows->set_external_layout_enabled(true);
	rows->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	rows->set_size(Size2(200, 1000));
	scroll->get_content()->add_child(rows);
	auto host = descriptor("RCTScrollView");
	Dictionary props;
	auto state = prepare("RCTScrollView", props);
	RNError error;
	REQUIRE(host->apply(scroll, state, RNHostContext(), error));
	scroll->publish(state, RNHostContext());
	auto wheel = [&]() {
		Ref<InputEventMouseButton> event;
		event.instantiate();
		event->set_position(Point2(50, 50));
		event->set_global_position(Point2(50, 50));
		event->set_button_index(MouseButton::WHEEL_DOWN);
		event->set_pressed(true);
		viewport->push_input(event, true);
	};
	wheel();
	REQUIRE(scroll->get_v_scroll() > 0);
	const auto original = host->capture_state(scroll);
	for (const char *mode : { "none", "box-none" }) {
		props["pointerEvents"] = mode;
		REQUIRE(host->apply(scroll, prepare("RCTScrollView", props), RNHostContext(), error));
		const int before = scroll->get_v_scroll();
		wheel();
		CHECK(scroll->get_v_scroll() == before);
		CHECK(scroll->get_v_scroll_bar()->get_mouse_filter() == Control::MOUSE_FILTER_IGNORE);
	}
	host->restore_state(scroll, original);
	CHECK(scroll->get_v_scroll_bar()->get_mouse_filter() == Control::MOUSE_FILTER_STOP);
	const int before = scroll->get_v_scroll();
	wheel();
	CHECK(scroll->get_v_scroll() > before);
	memdelete(scroll);
}

TEST_CASE("[ReactNativeBindings][TextInput][SceneTree] native preedit defers the latest acknowledged replacement and rollback retains composition") {
	Dictionary props;
	props["text"] = "start";
	auto state = prepare("GodotTextInput", props);
	auto *control = memnew(RNTextInputControl);
	SceneTree::get_singleton()->get_root()->add_child(control);
	control->apply(state);
	control->publish(state, RNHostContext());
	auto *line = Object::cast_to<LineEdit>(control->get_editor());
	line->grab_focus();
	line->edit();
	line->notification(Node::NOTIFICATION_WM_WINDOW_FOCUS_IN);
	auto *display = static_cast<DisplayServerMock *>(DisplayServer::get_singleton());
	display->simulate_ime(String::utf8("に"), Vector2i(0, 1));
	line->notification(MainLoop::NOTIFICATION_OS_IME_UPDATE);
	REQUIRE(control->is_composing());
	control->reconcile(0, "older", 0, 0);
	control->reconcile(0, "latest", 0, 0);
	CHECK(control->get_text() == "start");
	auto checkpoint = control->capture_state();
	line->cancel_ime();
	control->restore_state(checkpoint);
	CHECK(control->is_composing());
	display->simulate_ime("", Vector2i());
	line->notification(MainLoop::NOTIFICATION_OS_IME_UPDATE);
	control->notification(Node::NOTIFICATION_PROCESS);
	CHECK(control->get_text() == "latest");
	CHECK(control->get_event_count() == 0);
	display->simulate_ime(String::utf8("に"), Vector2i(0, 1));
	line->notification(MainLoop::NOTIFICATION_OS_IME_UPDATE);
	control->reconcile(0, "superseded", 0, 0);
	line->cancel_ime();
	line->set_text("new native commit");
	line->emit_signal("text_changed", line->get_text());
	control->notification(Node::NOTIFICATION_PROCESS);
	CHECK(control->get_text() == "new native commit");
	CHECK(control->get_event_count() == 1);
	memdelete(control);
}

TEST_CASE("[ReactNativeBindings][ScrollView][SceneTree] independent content layout and native offsets use a constrained usable viewport") {
	auto *scroll = memnew(RNScrollControl);
	SceneTree::get_singleton()->get_root()->add_child(scroll);
	scroll->set_external_layout_enabled(true);
	scroll->set_size(Size2(240, 100));
	Dictionary props;
	props["horizontal"] = false;
	props["showsVerticalScrollIndicator"] = true;
	auto state = prepare("RCTScrollView", props);
	auto host = descriptor("RCTScrollView");
	RNError error;
	REQUIRE(host->apply(scroll, state, RNHostContext(), error));
	const Rect2 expected = host->get_child_layout_viewport(state, scroll->get_size());
	CHECK(expected.size.x < 240);
	CHECK(expected.size.y <= 100);
	CHECK(scroll->usable_viewport().size.is_equal_approx(expected.size));
	auto *content = memnew(Control);
	content->set_external_layout_enabled(true);
	content->set_size(Size2(expected.size.x, 1000));
	scroll->get_content()->add_child(content);
	scroll->publish(state, RNHostContext());
	scroll->scroll_to(Point2(0, 800), false);
	CHECK(scroll->get_v_scroll() == 800);
	const Variant retained = host->capture_state(scroll);
	scroll->set_offset(Point2());
	host->restore_state(scroll, retained);
	CHECK(scroll->get_v_scroll() == 800);
	scroll->publish(state, RNHostContext());
	CHECK(scroll->get_v_scroll() == 800);
	content->set_size(Size2(expected.size.x, 20));
	scroll->refresh_content();
	CHECK(scroll->get_v_scroll() == 0);
	memdelete(scroll);
}
TEST_CASE("[ReactNativeBindings][ScrollView][SceneTree] visible anchors follow retained rows and explicit offsets take precedence") {
	auto *scroll = memnew(RNScrollControl);
	SceneTree::get_singleton()->get_root()->add_child(scroll);
	scroll->set_external_layout_enabled(true);
	scroll->set_size(Size2(240, 100));
	Dictionary anchor;
	anchor["minIndexForVisible"] = 0;
	Dictionary props;
	props["maintainVisibleContentPosition"] = anchor;
	auto state = prepare("RCTScrollView", props);
	auto host = descriptor("RCTScrollView");
	RNError error;
	REQUIRE(host->apply(scroll, state, RNHostContext(), error));
	auto *rows = memnew(Control);
	rows->set_external_layout_enabled(true);
	rows->set_size(Size2(200, 1000));
	scroll->get_content()->add_child(rows);
	for (int i = 0; i < 3; ++i) {
		auto *row = memnew(Control);
		row->set_external_layout_enabled(true);
		row->set_position(Point2(0, i * 100));
		row->set_size(Size2(200, 100));
		rows->add_child(row);
	}
	scroll->publish(state, RNHostContext());
	scroll->scroll_to(Point2(0, 150), false);
	host->capture_state(scroll);
	for (int i = 0; i < rows->get_child_count(); ++i) {
		auto *row = Object::cast_to<Control>(rows->get_child(i));
		row->set_position(row->get_position() + Point2(0, 50));
	}
	scroll->publish(state, RNHostContext());
	CHECK(scroll->get_v_scroll() == 200);
	host->capture_state(scroll);
	Dictionary offset;
	offset["x"] = 0;
	offset["y"] = 25;
	props["contentOffset"] = offset;
	scroll->publish(prepare("RCTScrollView", props), RNHostContext());
	CHECK(scroll->get_v_scroll() == 25);
	memdelete(scroll);
}

TEST_CASE("[ReactNativeBindings][Presentation][SceneTree] nested modal stack preserves order and restores focus in its native window") {
	auto *background = memnew(LineEdit);
	const auto modal = descriptor("RCTModalHostView");
	Dictionary props;
	props["visible"] = true;
	auto state = prepare("RCTModalHostView", props);
	auto data = std::make_shared<RNPresentationData>();
	data->viewport = Size2(320, 200);
	state.component_data = data;
	auto *lower = Object::cast_to<RNModalControl>(modal->create_host(RNHostContext()));
	auto *upper = Object::cast_to<RNModalControl>(modal->create_host(RNHostContext()));
	SceneTree::get_singleton()->get_root()->add_child(lower);
	lower->add_child(background);
	lower->add_child(upper);
	background->grab_focus();
	modal->after_publish(lower, state, RNHostContext());
	CHECK(lower->is_top());
	CHECK(lower->get_overlay()->has_focus());
	CHECK(lower->get_overlay()->get_size() == data->viewport);
	modal->after_publish(upper, state, RNHostContext());
	CHECK(upper->is_top());
	CHECK(upper->get_overlay()->has_focus());
	CHECK_FALSE(lower->is_top());
	modal->after_publish(lower, state, RNHostContext());
	CHECK(upper->is_top());
	modal->dispose_state(lower, RNHostContext());
	CHECK(upper->is_top());
	modal->dispose_state(upper, RNHostContext());
	CHECK_FALSE(upper->is_top());
	CHECK(background->has_focus());
	memdelete(upper);
	memdelete(background);
	memdelete(lower);
	auto window_state = prepare("GodotWindow", Dictionary());
	const auto window_descriptor = descriptor("GodotWindow");
	auto *window = Object::cast_to<RNWindowControl>(window_descriptor->create_host(RNHostContext()));
	SceneTree::get_singleton()->get_root()->add_child(window);
	window_descriptor->after_publish(window, window_state, RNHostContext());
	CHECK(window->get_native_window()->get_size() == Size2i(640, 480));
	window->get_native_window()->set_size(Size2i(800, 600));
	CHECK(window->resolve_size(Size2(640, 480)) == Size2(800, 600));
	CHECK(window->resolve_size(Size2(480, 320)) == Size2(480, 320));
	memdelete(window);
}

TEST_CASE("[ReactNativeBindings][TextInput][SceneTree] initial zero prop revision is applied once through later native acknowledgments") {
	Dictionary props;
	props["text"] = "initial";
	auto state = prepare("GodotTextInput", props);
	auto *control = memnew(RNTextInputControl);
	control->apply(state);
	auto *line = Object::cast_to<LineEdit>(control->get_editor());
	line->set_text("native value");
	line->emit_signal("text_changed", line->get_text());
	props["mostRecentEventCount"] = 1;
	state = prepare("GodotTextInput", props);
	control->apply(state);
	CHECK(control->get_text() == "native value");
	props["multiline"] = true;
	state = prepare("GodotTextInput", props);
	control->apply(state);
	control->publish(state, RNHostContext());
	CHECK(Object::cast_to<TextEdit>(control->get_editor()) != nullptr);
	CHECK(control->get_text() == "native value");
	CHECK_FALSE(Object::cast_to<TextEdit>(control->get_editor())->has_undo());
	memdelete(control);
}

} //namespace TestRNComponents
