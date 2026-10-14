#pragma once

#include "../runtime/rn_service_settings.h"

#include "modules/websocket/websocket_peer.h"

#include <FBReactNativeSpec/FBReactNativeSpecJSI.h>

#include <map>
#include <memory>
#include <mutex>
#include <set>

class RNBlobService;
class RNHTTPService;
class RNMainThreadQueue;

namespace facebook::react {

// Owns the native sockets on the main thread and reports their events, which
// the WebSocketModule forwards to JavaScript as device events.
class RNWebSocketHub {
	struct Socket {
		Ref<WebSocketPeer> peer;
		bool opened = false;
		bool closing = false;
		bool forced = false;
		bool blob_messages = false;
		String failure;
		double close_deadline = 0;
	};
	std::map<int, Socket> sockets;
	std::shared_ptr<RNBlobService> blobs;
	RNServiceSettings settings;
	std::function<void(const std::string &, folly::dynamic)> emit_event;
	void _emit(const char *p_name, int p_id, folly::dynamic p_payload);

public:
	RNWebSocketHub(std::shared_ptr<RNBlobService> p_blobs, RNServiceSettings p_settings) :
			blobs(std::move(p_blobs)), settings(std::move(p_settings)) {}
	void set_event_emitter(std::function<void(const std::string &, folly::dynamic)> p_emit) { emit_event = std::move(p_emit); }
	void connect(int p_id, const String &p_url, const PackedStringArray &p_protocols, const PackedStringArray &p_headers);
	void send(int p_id, const PackedByteArray &p_bytes, bool p_text);
	void send_blob(int p_id, const Dictionary &p_blob);
	void close(int p_id, int p_code, const String &p_reason);
	void set_blob_messages(int p_id, bool p_enabled);
	void process_frame();
	void shutdown();
	size_t get_socket_count() const { return sockets.size(); }
};

struct RNNetworkModuleContext {
	std::shared_ptr<RNBlobService> blobs;
	std::shared_ptr<RNHTTPService> http;
	std::shared_ptr<RNWebSocketHub> sockets;
	std::shared_ptr<RNMainThreadQueue> main_thread;
};

class RNBlobModule : public NativeBlobModuleCxxSpec<RNBlobModule> {
	RNNetworkModuleContext context;

public:
	RNBlobModule(std::shared_ptr<CallInvoker> jsInvoker, RNNetworkModuleContext context);
	jsi::Object getConstants(jsi::Runtime &rt);
	void addNetworkingHandler(jsi::Runtime &rt) {}
	void addWebSocketHandler(jsi::Runtime &rt, double id);
	void removeWebSocketHandler(jsi::Runtime &rt, double id);
	void sendOverSocket(jsi::Runtime &rt, jsi::Object blob, double socketID);
	void createFromParts(jsi::Runtime &rt, jsi::Array parts, const std::string &withId);
	void release(jsi::Runtime &rt, const std::string &blobId);
};

class RNFileReaderModule : public NativeFileReaderModuleCxxSpec<RNFileReaderModule> {
	RNNetworkModuleContext context;
	jsi::Value _read(jsi::Runtime &rt, jsi::Object data, bool p_as_url, const std::string &p_encoding);

public:
	RNFileReaderModule(std::shared_ptr<CallInvoker> jsInvoker, RNNetworkModuleContext context);
	jsi::Value readAsDataURL(jsi::Runtime &rt, jsi::Object data) { return _read(rt, std::move(data), true, "utf-8"); }
	jsi::Value readAsText(jsi::Runtime &rt, jsi::Object data, const std::string &encoding) { return _read(rt, std::move(data), false, encoding); }
};

class RNNetworkingModule : public NativeNetworkingIOSCxxSpec<RNNetworkingModule> {
	RNNetworkModuleContext context;
	std::shared_ptr<std::mutex> mutex = std::make_shared<std::mutex>();
	std::shared_ptr<std::map<int, uint64_t>> requests = std::make_shared<std::map<int, uint64_t>>();
	int next_request = 1;
	void _emit(const char *p_name, folly::dynamic p_arguments);

public:
	RNNetworkingModule(std::shared_ptr<CallInvoker> jsInvoker, RNNetworkModuleContext context);
	void sendRequest(jsi::Runtime &rt, jsi::Object query, jsi::Function callback);
	void abortRequest(jsi::Runtime &rt, double requestId);
	void clearCookies(jsi::Runtime &rt, jsi::Function callback);
	void addListener(jsi::Runtime &rt, const std::string &eventName) {}
	void removeListeners(jsi::Runtime &rt, double count) {}
};

class RNWebSocketModule : public NativeWebSocketModuleCxxSpec<RNWebSocketModule> {
	RNNetworkModuleContext context;

public:
	RNWebSocketModule(std::shared_ptr<CallInvoker> jsInvoker, RNNetworkModuleContext context);
	void connect(jsi::Runtime &rt, const std::string &url, std::optional<jsi::Array> protocols, jsi::Object options, double socketID);
	void send(jsi::Runtime &rt, const std::string &message, double forSocketID);
	void sendBinary(jsi::Runtime &rt, const std::string &base64String, double forSocketID);
	void ping(jsi::Runtime &rt, double socketID);
	void close(jsi::Runtime &rt, double code, const std::string &reason, double socketID);
	void addListener(jsi::Runtime &rt, const std::string &eventName) {}
	void removeListeners(jsi::Runtime &rt, double count) {}
};

} // namespace facebook::react
