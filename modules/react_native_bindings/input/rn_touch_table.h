#pragma once

#include "rn_input_router.h"

// The contacts of the whole application. React Native lists every active
// touch in TouchEvent.touches and has one primary touch, whichever root each
// contact is in, so every root's router shares the host's table.
struct RNTouchTable {
	struct Contact {
		RNInputRouter::Target target;
		RNInputRouter::Position position;
		float pressure = 0.5f;
		ObjectID root;
	};

	HashMap<int, Contact> touches;
	// A pointer stays primary for its whole lifetime, so nothing becomes
	// primary again until every contact is released.
	int primary_touch = -1;
	// The mouse while its primary button presses a view, which React Native
	// also lists as a touch.
	Contact mouse;
};
