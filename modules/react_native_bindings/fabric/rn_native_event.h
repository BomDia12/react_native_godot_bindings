#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

#include <cstdint>
#include <memory>

class RNEventTarget;

struct RNNativeEvent {
	int root_tag = 0;
	int tag = 0;
	String name;
	int priority = 0;
	uint64_t generation = 0;
	uint64_t surface_epoch = 0;
	Dictionary payload;
	std::shared_ptr<RNEventTarget> retained_target;
};
