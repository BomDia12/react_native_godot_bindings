#pragma once

#include <react/renderer/components/view/BaseViewEventEmitter.h>

namespace facebook::react {

struct GodotKeyEvent {
	std::string key;
	std::string code;
	bool altKey{ false };
	bool ctrlKey{ false };
	bool metaKey{ false };
	bool shiftKey{ false };
	bool repeat{ false };
};

class HostPlatformViewEventEmitter : public BaseViewEventEmitter {
public:
	using BaseViewEventEmitter::BaseViewEventEmitter;

	void onMiddleClick(PointerEvent event) const;
	void onRightClick(PointerEvent event) const;
	void onKeyDown(const GodotKeyEvent &event) const;
	void onKeyUp(const GodotKeyEvent &event) const;
	void onGodotContextMenuAction(const std::string &id) const;
};

} // namespace facebook::react
