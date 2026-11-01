#include "../components/rn_view_control.h"
#include "../input/rn_input_event_value.h"
#include "../interop/rn_dynamic.h"
#include "../turbomodules/rn_async_storage_module.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "scene/resources/style_box_flat.h"
#include "tests/test_macros.h"

namespace TestRNGameUI {

TEST_CASE("[ReactNativeBindings][GameUI] input events round trip through their JavaScript form") {
	Ref<InputEventKey> key;
	key.instantiate();
	key->set_physical_keycode(Key::SPACE);
	key->set_keycode(Key::SPACE);
	key->set_ctrl_pressed(true);
	Dictionary value = rn_input_event_to_value(key);
	CHECK(String(value["type"]) == "key");
	CHECK_FALSE(String(value["label"]).is_empty());
	RNError error;
	const Ref<InputEventKey> parsed_key = rn_input_event_from_value(value, error);
	CHECK_FALSE(error.is_set());
	REQUIRE(parsed_key.is_valid());
	CHECK(parsed_key->get_physical_keycode() == Key::SPACE);
	CHECK(parsed_key->is_ctrl_pressed());
	CHECK_FALSE(parsed_key->is_alt_pressed());

	Ref<InputEventJoypadMotion> motion;
	motion.instantiate();
	motion->set_axis(JoyAxis::LEFT_X);
	motion->set_axis_value(-1);
	const Ref<InputEventJoypadMotion> parsed_motion = rn_input_event_from_value(rn_input_event_to_value(motion), error);
	REQUIRE(parsed_motion.is_valid());
	CHECK(parsed_motion->get_axis() == JoyAxis::LEFT_X);
	CHECK(parsed_motion->get_axis_value() == -1);

	Ref<InputEventJoypadButton> button;
	button.instantiate();
	button->set_button_index(JoyButton::A);
	const Ref<InputEventJoypadButton> parsed_button = rn_input_event_from_value(rn_input_event_to_value(button), error);
	REQUIRE(parsed_button.is_valid());
	CHECK(parsed_button->get_button_index() == JoyButton::A);

	Ref<InputEventMouseButton> mouse;
	mouse.instantiate();
	mouse->set_button_index(MouseButton::RIGHT);
	const Ref<InputEventMouseButton> parsed_mouse = rn_input_event_from_value(rn_input_event_to_value(mouse), error);
	REQUIRE(parsed_mouse.is_valid());
	CHECK(parsed_mouse->get_button_index() == MouseButton::RIGHT);

	Dictionary unknown;
	unknown["type"] = "wheel";
	CHECK(rn_input_event_from_value(unknown, error).is_null());
	CHECK(error.code == RNErrorCode::VALIDATION);
}

TEST_CASE("[ReactNativeBindings][GameUI] storage names stay inside the storage directory") {
	CHECK(rn_storage_name_is_valid("settings"));
	CHECK(rn_storage_name_is_valid("save_1.backup-2"));
	CHECK_FALSE(rn_storage_name_is_valid(""));
	CHECK_FALSE(rn_storage_name_is_valid("../escape"));
	CHECK_FALSE(rn_storage_name_is_valid(".."));
	CHECK_FALSE(rn_storage_name_is_valid("a/b"));
	CHECK_FALSE(rn_storage_name_is_valid("a\\b"));
}

TEST_CASE("[ReactNativeBindings][GameUI] storage merges objects recursively and replaces everything else") {
	Dictionary inner;
	inner["b"] = 1;
	Dictionary target;
	target["a"] = inner;
	target["c"] = 2;
	target["e"] = "old";
	Dictionary inner_source;
	inner_source["d"] = 3;
	Dictionary source;
	source["a"] = inner_source;
	source["e"] = "new";
	const Dictionary merged = rn_storage_merge(target, source);
	const Dictionary merged_inner = merged["a"];
	CHECK(int(merged_inner["b"]) == 1);
	CHECK(int(merged_inner["d"]) == 3);
	CHECK(int(merged["c"]) == 2);
	CHECK(String(merged["e"]) == "new");
	CHECK_FALSE(inner.has("d"));
}

TEST_CASE("[ReactNativeBindings][GameUI] storage files replace themselves and read back") {
	const String path = "user://rn_game_ui_test/settings.json";
	{
		RNStorageFile file(path);
		CHECK(file.load().is_empty());
		CHECK(file.get_values().is_empty());
		file.get_values()["volume"] = "0.5";
		CHECK(file.save().is_empty());
	}
	CHECK(FileAccess::exists(path));
	CHECK_FALSE(FileAccess::exists(path + ".tmp"));
	{
		RNStorageFile file(path);
		CHECK(file.load().is_empty());
		CHECK(String(file.get_values()["volume"]) == "0.5");
		file.get_values()["volume"] = "0.75";
		CHECK(file.save().is_empty());
	}
	{
		RNStorageFile file(path);
		CHECK(file.load().is_empty());
		CHECK(String(file.get_values()["volume"]) == "0.75");
	}
	{
		Ref<FileAccess> corrupt = FileAccess::open(path, FileAccess::WRITE);
		corrupt->store_string("not json");
	}
	ERR_PRINT_OFF;
	RNStorageFile corrupt(path);
	CHECK_FALSE(corrupt.load().is_empty());
	ERR_PRINT_ON;
	DirAccess::remove_absolute(path);
	DirAccess::remove_absolute("user://rn_game_ui_test");
}

TEST_CASE("[ReactNativeBindings][GameUI] shader parameters and drag data read Godot value wrappers") {
	folly::dynamic color = folly::dynamic::object("$godot", "Color")("r", 1)("g", 0.5)("b", 0);
	folly::dynamic position = folly::dynamic::object("$godot", "Vector2")("x", 3)("y", 4);
	const Dictionary converted = rn_typed_variant_from_dynamic(folly::dynamic::object("tint", color)("offset", position)("amount", 0.25));
	CHECK(Color(converted["tint"]).is_equal_approx(Color(1, 0.5, 0, 1)));
	CHECK(Vector2(converted["offset"]) == Vector2(3, 4));
	CHECK(double(converted["amount"]) == 0.25);
}

TEST_CASE("[ReactNativeBindings][GameUI] a view accepts only the drag types it declares") {
	RNViewControl *view = memnew(RNViewControl);
	Dictionary item;
	item["type"] = "item";
	Dictionary spell;
	spell["type"] = "spell";
	Array types;
	types.push_back("item");
	CHECK_FALSE(view->can_drop_data(Point2(), item));
	view->set_drag(Dictionary(), Ref<Texture2D>(), types);
	CHECK(view->can_drop_data(Point2(), item));
	CHECK_FALSE(view->can_drop_data(Point2(), spell));
	CHECK_FALSE(view->can_drop_data(Point2(), "item"));
	CHECK(view->get_drag_data(Point2()).get_type() == Variant::NIL);
	view->set_drag(item, Ref<Texture2D>(), Array());
	CHECK(view->get_drag_data(Point2()) == Variant(item));
	memdelete(view);
}

TEST_CASE("[ReactNativeBindings][GameUI] a skin replaces the React Native paint and clearing it restores the paint") {
	RNViewControl *view = memnew(RNViewControl);
	const Ref<StyleBox> paint = view->get_theme_stylebox("panel");
	CHECK(paint == view->get_style());
	Ref<StyleBoxFlat> skin;
	skin.instantiate();
	view->set_skin(skin, StringName());
	CHECK(view->get_theme_stylebox("panel") == skin);
	view->set_skin(Ref<StyleBox>(), "FramedPanel");
	CHECK_FALSE(view->has_theme_stylebox_override("panel"));
	CHECK(view->get_theme_type_variation() == StringName("FramedPanel"));
	view->set_skin(Ref<StyleBox>(), StringName());
	CHECK(view->get_theme_stylebox("panel") == view->get_style());
	memdelete(view);
}

} // namespace TestRNGameUI

void rn_force_link_game_ui_tests() {}
