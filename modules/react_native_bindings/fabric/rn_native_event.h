#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

#include <cstdint>

struct RNNativeEvent {
	int tag = 0;
	String name;
	int priority = 0;
	uint64_t generation = 0;
	Dictionary payload;
};
