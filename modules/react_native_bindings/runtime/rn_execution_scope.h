#pragma once

#include <cstdint>

struct RNExecutionOrigin {
	uint64_t generation = 0;
	int root_tag = 0;
	uint64_t surface_epoch = 0;
};
class RNJSNativeCallScope {
	static thread_local unsigned depth;

public:
	RNJSNativeCallScope() { ++depth; }
	~RNJSNativeCallScope() { --depth; }
	static bool active() { return depth != 0; }
};

class RNExecutionScope {
	RNExecutionOrigin previous;
	static thread_local RNExecutionOrigin origin;

public:
	explicit RNExecutionScope(const RNExecutionOrigin &p_origin);
	~RNExecutionScope();
	static RNExecutionOrigin current() { return origin; }
};
