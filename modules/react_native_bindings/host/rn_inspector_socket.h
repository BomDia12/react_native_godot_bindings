#pragma once

#include <react/http/IWebSocketClient.h>

#include <functional>
#include <optional>
#include <string>

namespace facebook::react {

// The reply that completes a DevTools Page.reload request relayed by Metro's
// inspector proxy, or nothing for any other message.
std::optional<std::string> rn_inspector_reload_reply(const std::string &p_message);

// Inspector connections that answer DevTools page reloads themselves and run
// p_reload, instead of passing them to upstream's in-place ReactHost reload,
// which would replace the runtime behind ReactNativeHost's back.
WebSocketClientFactory rn_inspector_web_socket_client_factory(WebSocketClientFactory p_factory, std::function<void()> p_reload);

} // namespace facebook::react
