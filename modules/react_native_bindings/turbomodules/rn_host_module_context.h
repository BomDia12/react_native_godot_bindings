#pragma once

#include <functional>
#include <memory>

class Object;
class RNMainThreadQueue;

// What Godot TurboModules need from the host: the main-thread queue, a
// main-thread lookup from a root tag to its running ReactNativeRootView, and
// main-thread requests to reload the runtime and to open the debugger.
struct RNHostModuleContext {
	std::shared_ptr<RNMainThreadQueue> main_thread;
	std::function<Object *(int p_root_tag)> find_root;
	std::function<void()> reload;
	std::function<void()> open_debugger;
};
