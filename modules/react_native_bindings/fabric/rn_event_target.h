#pragma once

#include <cstdint>
#include <memory>

namespace facebook::jsi {
class Runtime;
class Object;
class Value;
}

class RNEventTarget {
	int tag;
	uint64_t generation;
	class Impl;
	std::unique_ptr<Impl> impl;

public:
	RNEventTarget(int p_tag, uint64_t p_generation);
	RNEventTarget(int p_tag, uint64_t p_generation, facebook::jsi::Runtime &p_runtime, const facebook::jsi::Object &p_instance_handle);
	~RNEventTarget();

	int get_tag() const { return tag; }
	uint64_t get_generation() const { return generation; }
	bool has_instance_handle() const;

	facebook::jsi::Value lock(facebook::jsi::Runtime &p_runtime) const;
	void reset();
};
