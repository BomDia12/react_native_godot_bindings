#pragma once

#include "tests/display_server_mock.h"

class RNTestDisplayServer : public DisplayServerMock {
	String ime_text;
	Vector2i ime_selection;

public:
	String ime_get_text() const override { return ime_text; }
	Vector2i ime_get_selection() const override { return ime_selection; }

	void simulate_ime(const String &p_text, const Vector2i &p_selection) {
		ime_text = p_text;
		ime_selection = p_selection;
	}
};
