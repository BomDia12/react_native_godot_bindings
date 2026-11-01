#pragma once

#include "core/io/resource.h"
#include "core/string/ustring.h"

#include <cstdint>
#include <functional>

// Loads Godot resources that props name, on a resource-loader thread, so a large
// texture behind a StyleBox doesn't stall a mount. All of it runs on the main thread.
class RNResourceRequests {
public:
	using Completion = std::function<void(const Ref<Resource> &p_resource, const String &p_error)>;

	// A resource that's cached already, or a path that isn't valid, completes before
	// this returns, and the result is 0, as for no request at all.
	static uint64_t request(const String &p_path, const String &p_type_hint, Completion p_completion);
	static void cancel(uint64_t p_request);
	static void poll();
	static void shutdown();
};
