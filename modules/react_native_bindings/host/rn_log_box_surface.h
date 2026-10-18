#pragma once

#include "core/object/object_id.h"

#include <react/renderer/scheduler/SurfaceDelegate.h>

#include <atomic>
#include <memory>
#include <string>

class RNMainThreadQueue;

// Shows React Native's LogBox inspector in its own root view, on a CanvasLayer
// above the game. LogBox asks from the JS thread; the root view is created and
// freed on the main thread.
class RNLogBoxSurface : public facebook::react::SurfaceDelegate {
	std::shared_ptr<RNMainThreadQueue> main_thread;
	std::string application_key;
	std::atomic<bool> ready = false;
	std::atomic<bool> showing = false;
	ObjectID layer;

	void _open(const std::string &p_application_key);

public:
	explicit RNLogBoxSurface(std::shared_ptr<RNMainThreadQueue> p_main_thread);

	void createContentView(std::string p_application_key) override;
	bool isContentViewReady() override { return ready; }
	void destroyContentView() override;
	void show() override;
	void hide() override;
	bool isShowing() override { return showing; }

	void close();
};
