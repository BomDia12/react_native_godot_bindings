#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

#include <cstdint>

struct RNNativeEvent {
	int root_tag = 0;
	int tag = 0;
	String name;
	int priority = 0;
	uint64_t generation = 0;
	uint64_t surface_epoch = 0;
	Dictionary payload;
};
