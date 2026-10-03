#include "rn_websocket_module.h"

#include "rn_cookie_jar.h"

#include "core/crypto/crypto.h"
#include "core/os/os.h"

#include "modules/websocket/websocket_peer.h"

#include <algorithm>
#include <cstring>
#include <map>

namespace {
class RNWebSocketModule : public RNNativeModule {
	struct Socket {
		Ref<WebSocketPeer> peer;
		RNCallContext context;
		bool opened = false;
		bool closing = false;
		bool forced = false;
		double close_deadline = 0;
		int requested_code = 1000;
		String requested_reason;
	};
	std::map<int, Socket> sockets;
	std::function<RNServiceSettings()> settings_provider;
	std::function<std::shared_ptr<RNBlobService>()> blobs;
	RNServiceSettings settings;
	bool configured = false;
	bool emit(const Socket &p_socket, const String &p_event, int p_id, const Dictionary &p_payload) {
		auto registry = p_socket.context.registry.lock();
		if (!registry) {
			return false;
		}
		Dictionary event = p_payload.duplicate(true);
		event["id"] = p_id;
		Dictionary envelope;
		envelope["name"] = p_event;
		envelope["payload"] = event;
		return registry->queue_event("GodotWebSocket", "event", "", p_socket.context.generation, envelope);
	}
	RNModuleResult fail(const String &p_message, const String &p_code = RNErrorCode::VALIDATION) const { return RNModuleResult::failure(RNError::make(p_code, p_message, "WebSocket")); }

public:
	RNWebSocketModule(std::function<RNServiceSettings()> p_settings, std::function<std::shared_ptr<RNBlobService>()> p_blobs) :
			settings_provider(std::move(p_settings)), blobs(std::move(p_blobs)) {}
	RNModuleResult invoke_sync(const StringName &p_method, const Array &p_args, const RNCallContext &p_context) override {
		if (!configured) {
			settings = settings_provider();
			configured = true;
		}
		if (p_method == "stats") {
			Dictionary result;
			result["peers"] = int64_t(sockets.size());
			return RNModuleResult::success(result);
		}
		if (p_method == "connect") {
			if (int64_t(p_args[3]) < 0 || int64_t(p_args[3]) > INT32_MAX || p_args[2].get_type() != Variant::DICTIONARY) {
				return fail("Socket ID or headers are invalid");
			}
			const int id = p_args[3];
			if (sockets.count(id) || sockets.size() >= 128) {
				return fail("Socket ID is duplicate or peer limit is reached", RNErrorCode::LIMIT);
			}
			const String url = p_args[0];
			RNParsedURL parsed;
			if (!RNParsedURL::parse(url, parsed) || (parsed.scheme != "ws" && parsed.scheme != "wss")) {
				return fail("Socket URL must use ws or wss");
			}
			Socket socket;
			socket.context = p_context;
			socket.peer = Ref<WebSocketPeer>(WebSocketPeer::create());
			if (socket.peer.is_null()) {
				return fail("WebSocketPeer is unavailable", RNErrorCode::UNSUPPORTED);
			}
			socket.peer->set_inbound_buffer_size(settings.limit("network/websocket/max_buffered_bytes"));
			socket.peer->set_outbound_buffer_size(settings.limit("network/websocket/max_buffered_bytes"));
			socket.peer->set_max_queued_packets(settings.limit("network/websocket/max_queued_packets"));
			const Array protocols = p_args[1];
			PackedStringArray selected;
			for (const Variant &value : protocols) {
				if (value.get_type() != Variant::STRING || String(value).is_empty() || String(value).contains(",") || String(value).contains("\r") || String(value).contains("\n") || selected.has(value)) {
					return fail("Invalid socket subprotocol");
				}
				selected.push_back(value);
			}
			socket.peer->set_supported_protocols(selected);
			const Dictionary headers = p_args[2];
			PackedStringArray native_headers;
			for (const Variant &key : headers.keys()) {
				if (headers[key].get_type() != Variant::STRING || String(key).contains(":") || String(key).contains("\r") || String(key).contains("\n") || String(headers[key]).contains("\r") || String(headers[key]).contains("\n")) {
					return fail("Invalid handshake header");
				}
				native_headers.push_back(String(key) + ": " + String(headers[key]));
			}
			socket.peer->set_handshake_headers(native_headers);
			const Error error = socket.peer->connect_to_url(url, TLSOptions::client());
			if (error != OK) {
				Dictionary payload;
				payload["message"] = vformat("WebSocket connect failed (%d)", error);
				emit(socket, "websocketFailed", id, payload);
				return RNModuleResult::success();
			}
			sockets.emplace(id, std::move(socket));
			return RNModuleResult::success();
		}
		const int id = p_args[0];
		auto found = sockets.find(id);
		if (found == sockets.end()) {
			if (p_method == "close") {
				return RNModuleResult::success();
			}
			return fail("Socket is closed", RNErrorCode::STALE_HANDLE);
		}
		Socket &socket = found->second;
		if (p_method == "close") {
			const int code = p_args[1];
			const String reason = p_args[2];
			if ((code != 1000 && (code < 3000 || code > 4999)) || reason.utf8().length() > 123) {
				return fail("Invalid WebSocket close code or reason");
			}
			if (!socket.closing) {
				socket.closing = true;
				socket.requested_code = code;
				socket.requested_reason = reason;
				socket.close_deadline = double(OS::get_singleton()->get_ticks_usec()) / 1000 + settings.limit("network/websocket/close_timeout_ms");
				socket.peer->close(code, reason);
			}
			return RNModuleResult::success();
		}
		if (p_method == "bufferedAmount") {
			return RNModuleResult::success(socket.peer->get_current_outbound_buffered_amount());
		}
		if (socket.peer->get_ready_state() != WebSocketPeer::STATE_OPEN || socket.closing) {
			return fail("Socket is not open");
		}
		PackedByteArray bytes;
		bool text = false;
		if (p_method == "sendBlob") {
			const Dictionary data = p_args[1];
			const int64_t size = data.get("size", -1);
			if (size < 0 || size > settings.limit("network/websocket/max_message_bytes")) {
				return fail("Socket message limit exceeded", RNErrorCode::LIMIT);
			}
			auto binary = blobs();
			RNError error;
			if (!binary || !binary->pin(data, error)) {
				return RNModuleResult::failure(error.is_set() ? error : RNError::make(RNErrorCode::RUNTIME_RESET, "Binary service closed", "WebSocket"));
			}
			bytes.resize(size);
			for (int64_t offset = 0; offset < size; offset += 1024 * 1024) {
				PackedByteArray chunk;
				if (!binary->read(data, offset, std::min<int64_t>(1024 * 1024, size - offset), chunk, error)) {
					binary->unpin(data["blobId"]);
					return RNModuleResult::failure(error);
				}
				std::memcpy(bytes.ptrw() + offset, chunk.ptr(), chunk.size());
			}
			binary->unpin(data["blobId"]);
		} else {
			bytes = p_args[1];
			text = p_args[2];
		}
		if (bytes.size() > settings.limit("network/websocket/max_message_bytes") || bytes.size() > settings.limit("network/websocket/max_buffered_bytes") - socket.peer->get_current_outbound_buffered_amount()) {
			return fail("Socket outbound budget exceeded", RNErrorCode::LIMIT);
		}
		const Error error = socket.peer->send(bytes.ptr(), bytes.size(), text ? WebSocketPeer::WRITE_MODE_TEXT : WebSocketPeer::WRITE_MODE_BINARY);
		return error == OK ? RNModuleResult::success() : fail(vformat("Native socket send failed (%d)", error), RNErrorCode::NATIVE);
	}
	void process_frame(double) override {
		std::vector<int> closed;
		for (auto &entry : sockets) {
			Socket &socket = entry.second;
			socket.peer->poll();
			auto state = socket.peer->get_ready_state();
			if (state == WebSocketPeer::STATE_OPEN && !socket.opened && !socket.closing) {
				socket.opened = true;
				Dictionary payload;
				payload["protocol"] = socket.peer->get_selected_protocol();
				emit(socket, "websocketOpen", entry.first, payload);
			}
			int packets = 0;
			uint64_t bytes = 0;
			while (socket.peer->get_available_packet_count() > 0 && packets < settings.limit("network/websocket/packets_per_frame") && bytes < uint64_t(settings.limit("network/websocket/max_buffered_bytes"))) {
				auto registry = socket.context.registry.lock();
				if (!registry || !registry->can_queue_event(settings.limit("network/websocket/max_message_bytes") + 1024)) {
					break;
				}
				const uint8_t *data = nullptr;
				int size = 0;
				const Error error = socket.peer->get_packet(&data, size);
				if (error != OK || size > settings.limit("network/websocket/max_message_bytes")) {
					Dictionary payload;
					payload["message"] = "Socket inbound message exceeds limit or failed";
					emit(socket, "websocketFailed", entry.first, payload);
					socket.peer->close(-1);
					closed.push_back(entry.first);
					break;
				}
				PackedByteArray packet;
				packet.resize(size);
				if (size) {
					std::memcpy(packet.ptrw(), data, size);
				}
				Dictionary payload;
				payload["bytes"] = packet;
				payload["text"] = socket.peer->was_string_packet();
				emit(socket, "websocketMessage", entry.first, payload);
				++packets;
				bytes += size;
			}
			if (std::find(closed.begin(), closed.end(), entry.first) != closed.end()) {
				continue;
			}
			if (socket.closing && state != WebSocketPeer::STATE_CLOSED && double(OS::get_singleton()->get_ticks_usec()) / 1000 >= socket.close_deadline) {
				socket.forced = true;
				socket.peer->close(-1);
				state = WebSocketPeer::STATE_CLOSED;
			}
			if (state == WebSocketPeer::STATE_CLOSED) {
				Dictionary payload;
				const int code = socket.peer->get_close_code();
				if (!socket.opened && !socket.closing) {
					payload["message"] = "Socket handshake failed";
					emit(socket, "websocketFailed", entry.first, payload);
				} else {
					payload["code"] = code > 0 ? code : 1006;
					payload["reason"] = socket.peer->get_close_reason();
					payload["wasClean"] = socket.opened && !socket.forced && code > 0 && code != 1006;
					if (!emit(socket, "websocketClosed", entry.first, payload)) {
						continue;
					}
				}
				closed.push_back(entry.first);
			}
		}
		for (int id : closed) {
			sockets.erase(id);
		}
	}
	bool has_pending_work() const override { return !sockets.empty(); }
	void shutdown() override {
		for (auto &entry : sockets) {
			entry.second.peer->close(-1);
		}
		sockets.clear();
	}
};
} //namespace
bool rn_register_websocket_module(RNNativeModuleRegistry &p_registry, const std::function<RNServiceSettings()> &p_settings, const std::function<std::shared_ptr<RNBlobService>()> &p_blobs, RNError &r_error) {
	RNModuleDefinition definition;
	definition.name = "GodotWebSocket";
	definition.factory = [p_settings, p_blobs] { return std::make_unique<RNWebSocketModule>(p_settings, p_blobs); };
	for (const char *name : { "connect", "send", "sendBlob", "close", "bufferedAmount", "stats" }) {
		RNMethodSchema method;
		method.name = name;
		method.result = RNValueSchema::value(String(name) == "stats" ? RNValueType::DYNAMIC : String(name) == "bufferedAmount" ? RNValueType::INTEGER
																															   : RNValueType::VOID);
		Vector<RNValueType> types = String(name) == "stats" ? Vector<RNValueType>() : String(name) == "connect" ? Vector<RNValueType>({ RNValueType::STRING, RNValueType::ARRAY, RNValueType::DYNAMIC, RNValueType::INTEGER })
				: String(name) == "close"																		? Vector<RNValueType>({ RNValueType::INTEGER, RNValueType::INTEGER, RNValueType::STRING })
				: String(name) == "send"																		? Vector<RNValueType>({ RNValueType::INTEGER, RNValueType::BYTES, RNValueType::BOOL })
				: String(name) == "sendBlob"																	? Vector<RNValueType>({ RNValueType::INTEGER, RNValueType::DYNAMIC })
																												: Vector<RNValueType>({ RNValueType::INTEGER });
		for (int index = 0; index < types.size(); ++index) {
			RNArgumentSchema arg;
			arg.name = "arg" + itos(index);
			arg.value = types[index] == RNValueType::ARRAY ? RNValueSchema::array(RNValueSchema::value(RNValueType::STRING)) : RNValueSchema::value(types[index]);
			method.arguments.push_back(arg);
		}
		definition.methods.push_back(method);
	}
	RNEventSchema event;
	event.name = "event";
	event.subscription_name = "onEvent";
	event.payload = RNValueSchema::value(RNValueType::DYNAMIC);
	definition.events.push_back(event);
	return p_registry.register_module(definition, r_error);
}
