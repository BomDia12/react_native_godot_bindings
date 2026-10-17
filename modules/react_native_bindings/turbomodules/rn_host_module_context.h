#pragma once

#include <functional>
#include <memory>

class Object;
class RNMainThreadQueue;

// What Godot TurboModules need from the host: the main-thread queue, and a
// main-thread lookup from a root tag to its running ReactNativeRootView.
struct RNHostModuleContext {
	std::shared_ptr<RNMainThreadQueue> main_thread;
	std::function<Object *(int p_root_tag)> find_root;
};
