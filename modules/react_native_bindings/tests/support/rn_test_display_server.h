#pragma once

#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "tests/display_server_mock.h"

class RNTestDisplayServer : public DisplayServerMock {
	String ime_text;
	Vector2i ime_selection;

public:
	// -1 reports no screen-reader support, as headless platforms do.
	int screen_reader_active = -1;

	String ime_get_text() const override { return ime_text; }
	Vector2i ime_get_selection() const override { return ime_selection; }
	bool has_feature(DisplayServerEnums::Feature p_feature) const override {
		return (p_feature == DisplayServerEnums::FEATURE_ACCESSIBILITY_SCREEN_READER && screen_reader_active >= 0) || DisplayServerMock::has_feature(p_feature);
	}
	int accessibility_screen_reader_active() const override { return screen_reader_active; }
	// The scene tree finds the focused control to report through the window's instance.
	ObjectID window_get_attached_instance_id(DisplayServerEnums::WindowID p_window) const override {
		return p_window == DisplayServerEnums::MAIN_WINDOW_ID ? SceneTree::get_singleton()->get_root()->get_instance_id() : ObjectID();
	}

	void simulate_ime(const String &p_text, const Vector2i &p_selection) {
		ime_text = p_text;
		ime_selection = p_selection;
	}

	static RNTestDisplayServer *install() {
		if (RNTestDisplayServer *current = dynamic_cast<RNTestDisplayServer *>(DisplayServer::get_singleton())) {
			return current;
		}
		memdelete(DisplayServer::get_singleton());
		return memnew(RNTestDisplayServer);
	}
};
