#pragma once

#include "../host/rn_frame_clock.h"

#include "tests/test_macros.h"

namespace TestRNFrameClock {

int ticks_between(RNFrameClock &r_clock, double p_start_msec, double p_end_msec, double p_loop_msec, const RNFrameClock::Display &p_display) {
	int ticks = 0;
	for (double now = p_start_msec; now < p_end_msec; now += p_loop_msec) {
		ticks += r_clock.tick(uint64_t(now * 1000), p_display);
	}
	return ticks;
}

double average_rate(double p_loop_msec) {
	RNFrameClock clock;
	return ticks_between(clock, 0, 10000, p_loop_msec, { false, 60 }) / 10.0;
}

TEST_CASE("[ReactNativeBindings][FrameClock] only V-Sync that waits for the display presents every frame") {
	CHECK(RNFrameClock::presents(true, DisplayServerEnums::VSYNC_ENABLED));
	CHECK(RNFrameClock::presents(true, DisplayServerEnums::VSYNC_ADAPTIVE));
	CHECK_FALSE(RNFrameClock::presents(true, DisplayServerEnums::VSYNC_MAILBOX));
	CHECK_FALSE(RNFrameClock::presents(true, DisplayServerEnums::VSYNC_DISABLED));
	// Headless Godot cannot draw, whatever its V-Sync mode.
	CHECK_FALSE(RNFrameClock::presents(false, DisplayServerEnums::VSYNC_ENABLED));

	RNFrameClock clock;
	CHECK(ticks_between(clock, 0, 100, 1, { true, 60 }) == 100);
}

TEST_CASE("[ReactNativeBindings][FrameClock] an unknown refresh rate paces at 60 Hz") {
	CHECK(RNFrameClock::period_usec(0) == RNFrameClock::period_usec(60));
	CHECK(RNFrameClock::period_usec(-1) == RNFrameClock::period_usec(60));
	CHECK(RNFrameClock::period_usec(120) == 8333);
}

TEST_CASE("[ReactNativeBindings][FrameClock] an unpaced loop ticks at the refresh rate on average") {
	CHECK(Math::abs(average_rate(1) - 60) < 0.5);
	CHECK(Math::abs(average_rate(6.9) - 60) < 0.5);
	// A loop slower than the display ticks on every frame.
	CHECK(Math::abs(average_rate(20) - 50) < 0.5);
}

TEST_CASE("[ReactNativeBindings][FrameClock] a stall resynchronizes instead of catching up") {
	RNFrameClock clock;
	const RNFrameClock::Display display{ false, 60 };
	ticks_between(clock, 0, 1000, 6.9, display);
	CHECK(clock.tick(1500 * 1000, display));
	CHECK(ticks_between(clock, 1506.9, 1600, 6.9, display) <= 6);
}

} // namespace TestRNFrameClock
