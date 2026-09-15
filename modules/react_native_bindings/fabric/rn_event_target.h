#pragma once

#include <cstdint>
#include <memory>

namespace facebook::jsi {
class Runtime;
class Object;
class Value;
} //namespace facebook::jsi

class RNEventTarget {
	int tag;
	int root_tag;
	uint64_t generation;
	uint64_t surface_epoch;
	class Impl;
	std::unique_ptr<Impl> impl;

public:
	RNEventTarget(int p_tag, uint64_t p_generation, int p_root_tag = 0, uint64_t p_surface_epoch = 0);
	RNEventTarget(int p_tag, uint64_t p_generation, int p_root_tag, uint64_t p_surface_epoch, facebook::jsi::Runtime &p_runtime, const facebook::jsi::Object &p_instance_handle);
	~RNEventTarget();

	int get_tag() const { return tag; }
	int get_root_tag() const { return root_tag; }
	uint64_t get_generation() const { return generation; }
	uint64_t get_surface_epoch() const { return surface_epoch; }
	bool has_instance_handle() const;

	facebook::jsi::Value lock(facebook::jsi::Runtime &p_runtime) const;
	void reset();
	void set_instance_handle(facebook::jsi::Runtime &p_runtime, const facebook::jsi::Object &p_instance_handle);
};
