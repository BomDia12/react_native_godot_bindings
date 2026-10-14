#include "HostPlatformViewEventEmitter.h"

namespace facebook::react {

namespace {

folly::dynamic key_payload(const GodotKeyEvent &event) {
	return folly::dynamic::object("key", event.key)("code", event.code)("altKey", event.altKey)("ctrlKey", event.ctrlKey)("metaKey", event.metaKey)("shiftKey", event.shiftKey)("repeat", event.repeat)("isComposing", false);
}

} // namespace

void HostPlatformViewEventEmitter::onMiddleClick(PointerEvent event) const {
	const HighResTimeStamp timestamp = event.timeStamp;
	dispatchEvent("middleClick", std::make_shared<PointerEvent>(std::move(event)), RawEvent::Category::Discrete, timestamp);
}

void HostPlatformViewEventEmitter::onRightClick(PointerEvent event) const {
	const HighResTimeStamp timestamp = event.timeStamp;
	dispatchEvent("rightClick", std::make_shared<PointerEvent>(std::move(event)), RawEvent::Category::Discrete, timestamp);
}

void HostPlatformViewEventEmitter::onKeyDown(const GodotKeyEvent &event) const {
	dispatchEvent("keyDown", key_payload(event), RawEvent::Category::Discrete);
}

void HostPlatformViewEventEmitter::onKeyUp(const GodotKeyEvent &event) const {
	dispatchEvent("keyUp", key_payload(event), RawEvent::Category::Discrete);
}

void HostPlatformViewEventEmitter::onGodotContextMenuAction(const std::string &id) const {
	dispatchEvent("godotContextMenuAction", folly::dynamic::object("id", id), RawEvent::Category::Discrete);
}

} // namespace facebook::react
