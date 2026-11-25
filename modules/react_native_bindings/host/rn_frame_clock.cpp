#include "rn_frame_clock.h"

#include "servers/display/display_server.h"

RNFrameClock::Display RNFrameClock::Display::current() {
	const DisplayServer *display = DisplayServer::get_singleton();
	return { presents(display->window_can_draw(), display->window_get_vsync_mode(DisplayServerEnums::MAIN_WINDOW_ID)), display->screen_get_refresh_rate() };
}

// Mailbox replaces frames the display has not shown yet, so it does not pace the loop.
bool RNFrameClock::presents(bool p_can_draw, DisplayServerEnums::VSyncMode p_mode) {
	return p_can_draw && (p_mode == DisplayServerEnums::VSYNC_ENABLED || p_mode == DisplayServerEnums::VSYNC_ADAPTIVE);
}

uint64_t RNFrameClock::period_usec(double p_refresh_rate) {
	return uint64_t(1e6 / (p_refresh_rate > 0 ? p_refresh_rate : 60));
}

bool RNFrameClock::tick(uint64_t p_now_usec, const Display &p_display) {
	if (p_display.presented) {
		next_usec = p_now_usec;
		return true;
	}
	if (p_now_usec < next_usec) {
		return false;
	}
	const uint64_t period = period_usec(p_display.refresh_rate);
	// After a stall longer than a period, ticks resume from now instead of catching up.
	next_usec = p_now_usec - next_usec > period ? p_now_usec + period : next_usec + period;
	return true;
}
