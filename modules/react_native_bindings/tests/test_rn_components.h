#pragma once

#include "../components/rn_image_control.h"
#include "../components/rn_presentation_control.h"
#include "../components/rn_scroll_control.h"
#include "../components/rn_small_controls.h"
#include "../components/rn_text_control.h"
#include "../components/rn_text_input_control.h"
#include "../native_modules/rn_image_probe.h"
#include "support/rn_test_display_server.h"

#include "core/crypto/crypto_core.h"
#include "core/input/input.h"
#include "core/object/message_queue.h"
#include "scene/main/scene_tree.h"
#include "tests/test_macros.h"

namespace TestRNComponents {
RNTextInputConfig input_config(bool p_multiline = false) {
	RNTextInputConfig config;
	config.multiline = p_multiline;
	return config;
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
TEST_CASE("[ReactNativeBindings][Images] header probes read PNG JPEG and WebP dimensions and reject truncated or oversized headers") {
	RNImageHeader header;
	const auto png = png_bytes();
	REQUIRE(rn_probe_image(png.ptr(), png.size(), header));
	CHECK(header.format == RNImageFormat::PNG);
	CHECK(header.size == Size2i(2, 2));
	CHECK_FALSE(rn_probe_image(png.ptr(), 20, header));

	Ref<Image> image = Image::create_empty(3, 5, false, Image::FORMAT_RGB8);
	const Vector<uint8_t> jpeg = image->save_jpg_to_buffer();
	REQUIRE(rn_probe_image(jpeg.ptr(), jpeg.size(), header));
	CHECK(header.format == RNImageFormat::JPEG);
	CHECK(header.size == Size2i(3, 5));
	CHECK_FALSE(rn_probe_image(jpeg.ptr(), 16, header));

	for (const bool lossy : { false, true }) {
		const Vector<uint8_t> webp = image->save_webp_to_buffer(lossy);
		REQUIRE(rn_probe_image(webp.ptr(), webp.size(), header));
		CHECK(header.format == RNImageFormat::WEBP);
		CHECK(header.size == Size2i(3, 5));
		CHECK_FALSE(rn_probe_image(webp.ptr(), 24, header));
	}

	Vector<uint8_t> oversized = png;
	oversized.write[16] = 0x7F;
	CHECK_FALSE(rn_probe_image(oversized.ptr(), oversized.size(), header));
	Vector<uint8_t> empty = png;
	for (int i = 16; i < 20; ++i) {
		empty.write[i] = 0;
	}
	CHECK_FALSE(rn_probe_image(empty.ptr(), empty.size(), header));
}
TEST_CASE("[ReactNativeBindings][Images] decoded pixel budget and reservation lifetime precede allocation") {
	{
		RNImageLimits tight;
		tight.decoded_bytes = 4;
		auto service = std::make_shared<RNImageService>(tight);
		RNImageSource normalized;
		RNError error;
		REQUIRE(RNImageService::normalize(source(data_uri()), normalized, error));
		RNImageResult rejected;
		service->request(normalized, [&rejected](RNImageResult loaded) { rejected = loaded; });
		CHECK(rejected.error.code == RNErrorCode::LIMIT);
		CHECK(service->used_bytes() == 0);
	}
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
TEST_CASE("[ReactNativeBindings][Text][SceneTree] text sized before insertion shapes at its final width") {
	auto *text = memnew(RNTextControl);
	float shaped_width = -1;
	text->set_content("centered", [&shaped_width](float p_width, RNParagraphLayout &) { shaped_width = p_width; }, false);
	text->set_size(Size2(380, 40));
	CHECK(shaped_width == 0);
	SceneTree::get_singleton()->get_root()->add_child(text);
	CHECK(shaped_width == 380);
	memdelete(text);
}
TEST_CASE("[ReactNativeBindings][TextInput][SceneTree] stale deferred editor signals for JS-applied values are ignored") {
	auto *control = memnew(RNTextInputControl);
	SceneTree::get_singleton()->get_root()->add_child(control);
	control->configure(input_config());
	control->reconcile(0, "start", -1, -1);
	auto *line = Object::cast_to<LineEdit>(control->get_editor());
	line->set_caret_column(5);
	line->insert_text_at_caret("!");
	line->emit_signal("text_changed", line->get_text());
	REQUIRE(control->get_event_count() == 1);
	control->reconcile(1, "applied", 2, 4);
	MessageQueue::get_singleton()->flush();
	CHECK(control->get_text() == "applied");
	CHECK(control->get_event_count() == 1);
	CHECK(control->get_selection() == Point2i(2, 4));

	control->configure(input_config(true));
	auto *edit = Object::cast_to<TextEdit>(control->get_editor());
	REQUIRE(edit);
	edit->insert_text_at_caret("?");
	edit->emit_signal("text_changed");
	REQUIRE(control->get_event_count() == 2);
	control->reconcile(2, "multi", 1, 3);
	MessageQueue::get_singleton()->flush();
	control->notification(Node::NOTIFICATION_PROCESS);
	CHECK(control->get_text() == "multi");
	CHECK(control->get_event_count() == 2);
	CHECK(control->get_selection() == Point2i(1, 3));
	memdelete(control);
}
TEST_CASE("[ReactNativeBindings][TextInput][SceneTree] acknowledgments use UTF16 and stale replacements lose to native edits") {
	RNTextInputControl *control = memnew(RNTextInputControl);
	control->configure(input_config());
	control->reconcile(0, String::utf8("A😀z"), -1, -1);
	CHECK(control->get_text() == String::utf8("A😀z"));
	control->reconcile(0, Variant(), 1, 3);
	CHECK(control->get_selection() == Point2i(1, 3));
	auto *line = Object::cast_to<LineEdit>(control->get_editor());
	line->set_text("native");
	line->emit_signal("text_changed", "native");
	CHECK(control->get_event_count() == 1);
	control->reconcile(0, "stale", 0, 0);
	CHECK(control->get_text() == "native");
	control->reconcile(1, "accepted", 0, 0);
	CHECK(control->get_text() == "accepted");
	const ObjectID single_line = control->get_editor()->get_instance_id();
	control->configure(input_config(true));
	CHECK(Object::cast_to<TextEdit>(control->get_editor()) != nullptr);
	CHECK(control->get_editor()->get_instance_id() != single_line);
	CHECK(control->get_text() == "accepted");
	CHECK_FALSE(Object::cast_to<TextEdit>(control->get_editor())->has_undo());
	memdelete(control);
}
TEST_CASE("[ReactNativeBindings][SmallControls][SceneTree] controlled switch reports only native changes") {
	auto *control = memnew(RNSwitchControl);
	int events = 0;
	control->set_changed_callback([&events](bool p_value) {
		CHECK(p_value);
		++events;
	});
	control->set_pressed_no_signal(false);
	control->set_pressed(true);
	CHECK(events == 1);
	control->set_pressed_no_signal(false);
	CHECK(events == 1);
	memdelete(control);
}
TEST_CASE("[ReactNativeBindings][TextInput][SceneTree] pointer rejection covers the wrapper and both native editors") {
	auto *control = memnew(RNTextInputControl);
	SceneTree::get_singleton()->get_root()->add_child(control);
	RNTextInputConfig config = input_config();
	config.targetable = false;
	control->configure(config);
	control->reconcile(0, "Retained", -1, -1);
	CHECK(control->get_mouse_filter() == Control::MOUSE_FILTER_IGNORE);
	CHECK(control->get_editor()->get_mouse_filter() == Control::MOUSE_FILTER_IGNORE);
	CHECK(Object::cast_to<LineEdit>(control->get_editor())->is_editable());
	config.multiline = true;
	control->configure(config);
	CHECK(Object::cast_to<TextEdit>(control->get_editor())->is_editable());
	CHECK(control->get_editor()->get_mouse_filter() == Control::MOUSE_FILTER_IGNORE);
	CHECK(control->get_text() == "Retained");
	config.targetable = true;
	control->configure(config);
	CHECK(control->get_mouse_filter() == Control::MOUSE_FILTER_PASS);
	CHECK(control->get_editor()->get_mouse_filter() == Control::MOUSE_FILTER_STOP);
	memdelete(control);
}
TEST_CASE("[ReactNativeBindings][TextInput][SceneTree] autofocus runs once after the first configuration and preserves later native focus") {
	auto *control = memnew(RNTextInputControl);
	auto *peer = memnew(LineEdit);
	SceneTree::get_singleton()->get_root()->add_child(control);
	SceneTree::get_singleton()->get_root()->add_child(peer);
	RNTextInputConfig config = input_config();
	config.auto_focus = true;
	peer->grab_focus();
	control->configure(config);
	CHECK(control->get_editor()->has_focus());
	peer->grab_focus();
	control->configure(config);
	CHECK(peer->has_focus());
	config.multiline = true;
	control->configure(config);
	CHECK(peer->has_focus());
	memdelete(control);
	memdelete(peer);
}
TEST_CASE("[ReactNativeBindings][TextInput][SceneTree] native preedit defers the latest acknowledged replacement") {
	memdelete(DisplayServer::get_singleton());
	RNTestDisplayServer *display = memnew(RNTestDisplayServer);
	auto *control = memnew(RNTextInputControl);
	SceneTree::get_singleton()->get_root()->add_child(control);
	control->configure(input_config());
	control->reconcile(0, "start", -1, -1);
	auto *line = Object::cast_to<LineEdit>(control->get_editor());
	line->grab_focus();
	line->edit();
	line->notification(Node::NOTIFICATION_WM_WINDOW_FOCUS_IN);
	display->simulate_ime(String::utf8("に"), Vector2i(0, 1));
	line->notification(MainLoop::NOTIFICATION_OS_IME_UPDATE);
	REQUIRE(control->is_composing());
	control->reconcile(0, "older", 0, 0);
	control->reconcile(0, "latest", 0, 0);
	CHECK(control->get_text() == "start");
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
TEST_CASE("[ReactNativeBindings][ScrollView][SceneTree] content taller than the viewport scrolls and short content resets the offset") {
	auto *scroll = memnew(RNScrollControl);
	SceneTree::get_singleton()->get_root()->add_child(scroll);
	scroll->set_size(Size2(240, 100));
	scroll->set_overlay_indicators(false, true);
	CHECK(scroll->usable_viewport().size.is_equal_approx(Size2(240, 100) - scroll->get_theme_stylebox("panel", "ScrollContainer")->get_minimum_size()));
	scroll->set_content_extent(Size2(240, 1000));
	scroll->scroll_to(Point2(0, 800), false);
	CHECK(scroll->get_v_scroll() == 800);
	// ScrollContainer also sorts the internal indicators with the content.
	scroll->notification(Container::NOTIFICATION_SORT_CHILDREN);
	auto *indicators = Object::cast_to<RNScrollIndicators>(scroll->get_child(scroll->get_child_count(true) - 1, true));
	REQUIRE(indicators);
	CHECK(indicators->get_rect() == Rect2(Point2(), scroll->get_size()));
	scroll->set_content_extent(Size2(240, 20));
	scroll->refresh_content();
	CHECK(scroll->get_v_scroll() == 0);
	memdelete(scroll);
}
TEST_CASE("[ReactNativeBindings][ScrollView][SceneTree] visible anchors follow retained rows") {
	auto *scroll = memnew(RNScrollControl);
	SceneTree::get_singleton()->get_root()->add_child(scroll);
	scroll->set_size(Size2(240, 100));
	RNScrollSettings settings;
	settings.maintain_visible_position = true;
	scroll->set_settings(settings);
	scroll->set_content_extent(Size2(200, 1000));
	auto *rows = memnew(Control);
	rows->set_size(Size2(200, 1000));
	scroll->get_content()->add_child(rows);
	for (int i = 0; i < 3; ++i) {
		auto *row = memnew(Control);
		row->set_position(Point2(0, i * 100));
		row->set_size(Size2(200, 100));
		rows->add_child(row);
	}
	scroll->scroll_to(Point2(0, 150), false);
	scroll->prepare_anchor();
	for (int i = 0; i < rows->get_child_count(); ++i) {
		auto *row = Object::cast_to<Control>(rows->get_child(i));
		row->set_position(row->get_position() + Point2(0, 50));
	}
	scroll->restore_visible_anchor();
	CHECK(scroll->get_v_scroll() == 200);
	memdelete(scroll);
}
TEST_CASE("[ReactNativeBindings][Presentation][SceneTree] nested modal stack preserves order and restores focus in its native window") {
	auto *background = memnew(LineEdit);
	auto *lower = memnew(RNModalControl);
	auto *upper = memnew(RNModalControl);
	SceneTree::get_singleton()->get_root()->add_child(lower);
	lower->add_child(background);
	lower->add_child(upper);
	background->grab_focus();
	const Size2 viewport(320, 200);
	lower->present(true, Color(), viewport);
	CHECK(lower->is_top());
	CHECK(lower->get_overlay()->has_focus());
	CHECK(lower->get_overlay()->get_size() == viewport);
	upper->present(true, Color(), viewport);
	CHECK(upper->is_top());
	CHECK(upper->get_overlay()->has_focus());
	CHECK_FALSE(lower->is_top());
	lower->present(true, Color(), viewport);
	CHECK(upper->is_top());
	lower->dismiss();
	CHECK(upper->is_top());
	upper->dismiss();
	CHECK_FALSE(upper->is_top());
	CHECK(background->has_focus());
	memdelete(upper);
	memdelete(background);
	memdelete(lower);
	auto *window = memnew(RNWindowControl);
	SceneTree::get_singleton()->get_root()->add_child(window);
	window->present("React Native", Size2(640, 480), Size2(640, 480), true);
	CHECK(window->get_native_window()->get_size() == Size2i(640, 480));
	window->get_native_window()->set_size(Size2i(800, 600));
	CHECK(window->resolve_size(Size2(640, 480)) == Size2(800, 600));
	CHECK(window->resolve_size(Size2(480, 320)) == Size2(480, 320));
	memdelete(window);
}
TEST_CASE("[ReactNativeBindings][ScrollView][SceneTree] drag and momentum phases follow the stock scroll signals") {
	Window *viewport = SceneTree::get_singleton()->get_root();
	viewport->set_size(Size2i(640, 480));
	Input::get_singleton()->set_emulate_touch_from_mouse(true);
	auto *scroll = memnew(RNScrollControl);
	viewport->add_child(scroll);
	scroll->set_size(Size2(240, 100));
	scroll->set_content_extent(Size2(200, 1000));
	Vector<StringName> phases;
	RNScrollCallbacks callbacks;
	callbacks.event = [&phases](const StringName &p_name) {
		if (p_name != StringName("topScroll")) {
			phases.push_back(p_name);
		}
	};
	scroll->set_callbacks(callbacks);
	REQUIRE(scroll->get_v_scroll_bar()->get_max() > 100);

	auto button = [&](bool p_pressed, const Point2 &p_position) {
		Ref<InputEventMouseButton> event;
		event.instantiate();
		event->set_position(p_position);
		event->set_global_position(p_position);
		event->set_button_index(MouseButton::LEFT);
		event->set_pressed(p_pressed);
		viewport->push_input(event, true);
	};
	auto motion = [&](const Point2 &p_position, const Vector2 &p_relative) {
		Ref<InputEventMouseMotion> event;
		event.instantiate();
		event->set_position(p_position);
		event->set_global_position(p_position);
		event->set_relative(p_relative);
		event->set_button_mask(MouseButtonMask::LEFT);
		viewport->push_input(event, true);
	};
	button(true, Point2(50, 80));
	motion(Point2(50, 40), Vector2(0, -40));
	button(false, Point2(50, 40));
	CHECK(phases == Vector<StringName>({ "topScrollBeginDrag", "topScrollEndDrag" }));
	CHECK(scroll->get_v_scroll() > 0);

	phases.clear();
	scroll->emit_signal("scroll_started");
	button(true, Point2(50, 40));
	button(false, Point2(50, 40));
	scroll->emit_signal("scroll_ended");
	CHECK(phases == Vector<StringName>({ "topScrollBeginDrag", "topScrollEndDrag", "topMomentumScrollBegin", "topMomentumScrollEnd" }));
	Input::get_singleton()->set_emulate_touch_from_mouse(false);
	memdelete(scroll);
}

} //namespace TestRNComponents
