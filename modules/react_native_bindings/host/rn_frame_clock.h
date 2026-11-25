#pragma once

#include "servers/display/display_server_enums.h"

#include <cstdint>

// Decides on the main thread, once per Godot frame, whether the frame is a
// display frame for requestAnimationFrame. A loop that V-Sync paces presents
// every frame. Otherwise, as headless or with V-Sync off, ticks keep the
// refresh rate on average whatever the loop rate.
class RNFrameClock {
	uint64_t next_usec = 0;

public:
	struct Display {
		bool presented = false;
		double refresh_rate = 0;

		static Display current();
	};

	static bool presents(bool p_can_draw, DisplayServerEnums::VSyncMode p_mode);
	static uint64_t period_usec(double p_refresh_rate);
	bool tick(uint64_t p_now_usec, const Display &p_display);
};
