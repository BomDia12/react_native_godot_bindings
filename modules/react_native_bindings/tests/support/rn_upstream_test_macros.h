#pragma once

// React Native headers pull in glog, whose CHECK macros collide with doctest's.
// Tests include this after upstream headers so the doctest macros win.
#undef CHECK
#undef CHECK_EQ
#undef CHECK_NE
#undef CHECK_GT
#undef CHECK_LT
#undef CHECK_GE
#undef CHECK_LE

#include "tests/test_macros.h"
