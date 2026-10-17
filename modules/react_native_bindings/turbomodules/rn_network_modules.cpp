#include "rn_network_modules.h"

#include "../host/rn_main_thread_queue.h"
#include "../interop/rn_dynamic.h"
#include "../native_modules/rn_blob_service.h"
#include "../native_modules/rn_http_service.h"
#include "rn_js_promise.h"

#include "core/crypto/crypto_core.h"
#include "core/os/os.h"

#include <jsi/JSIDynamic.h>

#include <cstring>

namespace facebook::react {

namespace {

constexpr int64_t CHUNK_BYTES = 1024 * 1024;

String godot_string(const std::string &p_value) {
	return String::utf8(p_value.c_str(), p_value.size());
}

PackedByteArray utf8_bytes(const std::string &p_value) {
	PackedByteArray bytes;
	bytes.resize(p_value.size());
	if (!p_value.empty()) {
		memcpy(bytes.ptrw(), p_value.data(), p_value.size());
	}
	return bytes;
}

int utf8_sequence_length(uint8_t p_lead) {
	if (p_lead < 0x80) {
		return 1;
	}
	if (p_lead < 0xC2) {
		return 0;
	}
	if (p_lead < 0xE0) {
		return 2;
	}
	if (p_lead < 0xF0) {
		return 3;
	}
	return p_lead < 0xF5 ? 4 : 0;
}

// Decodes bytes the way the WHATWG UTF-8 decoder does: each maximal invalid
// subsequence becomes one U+FFFD. Hermes would silently drop those bytes.
std::string decoded_utf8(const PackedByteArray &p_bytes) {
	const uint8_t *bytes = p_bytes.ptr();
	const int64_t size = p_bytes.size();
	std::string text;
	text.reserve(size);
	int64_t i = 0;
	while (i < size) {
		const uint8_t lead = bytes[i];
		const int length = utf8_sequence_length(lead);
		uint8_t second_min = 0x80;
		uint8_t second_max = 0xBF;
		if (lead == 0xE0) {
			second_min = 0xA0;
		} else if (lead == 0xF0) {
			second_min = 0x90;
		} else if (lead == 0xED) {
			second_max = 0x9F;
		} else if (lead == 0xF4) {
			second_max = 0x8F;
		}
		int valid = length > 0 ? 1 : 0;
		while (valid > 0 && valid < length && i + valid < size) {
			const uint8_t next = bytes[i + valid];
			if (next < (valid == 1 ? second_min : 0x80) || next > (valid == 1 ? second_max : 0xBF)) {
				break;
			}
			++valid;
		}
		if (length > 0 && valid == length) {
			text.append(reinterpret_cast<const char *>(bytes + i), length);
			i += length;
		} else {
			text.append("\xEF\xBF\xBD");
			i += MAX(valid, 1);
		}
	}
	return text;
}

PackedByteArray base64_bytes(const std::string &p_value) {
	PackedByteArray bytes;
	bytes.resize(((p_value.size() + 3) / 4) * 3);
	size_t decoded = 0;
	if (CryptoCore::b64_decode(bytes.ptrw(), bytes.size(), &decoded, reinterpret_cast<const uint8_t *>(p_value.data()), p_value.size()) != OK) {
		return PackedByteArray();
	}
	bytes.resize(decoded);
	return bytes;
}

Dictionary blob_descriptor(const folly::dynamic &p_data) {
	Dictionary descriptor;
	descriptor["blobId"] = godot_string(p_data["blobId"].asString());
	descriptor["offset"] = p_data.getDefault("offset", 0).asInt();
	descriptor["size"] = p_data.getDefault("size", 0).asInt();
	descriptor["type"] = godot_string(p_data.getDefault("type", "").asString());
	return descriptor;
}

bool read_blob(const std::shared_ptr<RNBlobService> &p_blobs, const Dictionary &p_descriptor, PackedByteArray &r_bytes, RNError &r_error) {
	if (!p_blobs->pin(p_descriptor, r_error)) {
		return false;
	}
	const int64_t size = p_descriptor.get("size", 0);
	r_bytes.resize(size);
	bool read = true;
	for (int64_t offset = 0; offset < size && read; offset += CHUNK_BYTES) {
		PackedByteArray chunk;
		read = p_blobs->read(p_descriptor, offset, MIN(CHUNK_BYTES, size - offset), chunk, r_error);
		if (read) {
			memcpy(r_bytes.ptrw() + offset, chunk.ptr(), chunk.size());
		}
	}
	p_blobs->unpin(p_descriptor["blobId"]);
	return read;
}

// Parses the URLs built by upstream URL.createObjectURL: blob:<id>?offset=<n>&size=<n>.
Dictionary blob_url_descriptor(const String &p_url) {
	const String path = p_url.trim_prefix("blob:");
	Dictionary descriptor;
	descriptor["blobId"] = path.get_slicec('?', 0);
	for (const String &parameter : path.get_slicec('?', 1).split("&", false)) {
		const String name = parameter.get_slicec('=', 0);
		if (name == "offset" || name == "size") {
			descriptor[name] = parameter.get_slicec('=', 1).to_int();
		}
	}
	return descriptor;
}

folly::dynamic blob_part_data(const folly::dynamic &p_part) {
	for (const char *key : { "blob", "data", "_data" }) {
		const folly::dynamic *value = p_part.get_ptr(key);
		if (value && value->isObject() && value->get_ptr("blobId")) {
			return *value;
		}
	}
	return nullptr;
}

// Converts XMLHttpRequest's query into the request record the Godot HTTP service builds from.
Dictionary http_record(const folly::dynamic &p_query) {
	Dictionary record;
	record["url"] = godot_string(p_query["url"].asString());
	record["method"] = godot_string(p_query.getDefault("method", "GET").asString());
	record["timeout"] = p_query.getDefault("timeout", 0).asDouble();
	record["credentials"] = p_query.getDefault("withCredentials", true).asBool();
	Dictionary headers;
	if (const folly::dynamic *values = p_query.get_ptr("headers"); values && values->isObject()) {
		for (const auto &[name, value] : values->items()) {
			headers[godot_string(name.asString())] = godot_string(value.isString() ? value.getString() : value.asString());
		}
	}
	record["headers"] = headers;
	const folly::dynamic *data = p_query.get_ptr("data");
	if (!data || !data->isObject()) {
		return record;
	}
	if (const folly::dynamic *text = data->get_ptr("string")) {
		record["body"] = utf8_bytes(text->asString());
	} else if (const folly::dynamic *base64 = data->get_ptr("base64")) {
		record["body"] = base64_bytes(base64->asString());
	} else if (const folly::dynamic *blob = data->get_ptr("blob")) {
		record["body"] = blob_descriptor(*blob);
	} else if (const folly::dynamic *uri = data->get_ptr("uri")) {
		Dictionary file;
		file["uri"] = godot_string(uri->asString());
		record["body"] = file;
	} else if (const folly::dynamic *form = data->get_ptr("formData"); form && form->isArray()) {
		Array parts;
		for (const folly::dynamic &part : *form) {
			Dictionary converted;
			converted["headers"] = rn_variant_from_dynamic(part.getDefault("headers", folly::dynamic::object()));
			const folly::dynamic blob_data = blob_part_data(part);
			if (!blob_data.isNull()) {
				converted["blob"] = blob_descriptor(blob_data);
			} else if (const folly::dynamic *part_uri = part.get_ptr("uri")) {
				converted["uri"] = godot_string(part_uri->asString());
			} else {
				converted["bytes"] = utf8_bytes(part.getDefault("string", "").asString());
			}
			parts.push_back(converted);
		}
		record["formData"] = parts;
	}
	return record;
}

} // namespace

void RNWebSocketHub::_emit(const char *p_name, int p_id, folly::dynamic p_payload) {
	p_payload["id"] = p_id;
	if (emit_event) {
		emit_event(p_name, std::move(p_payload));
	}
}

void RNWebSocketHub::connect(int p_id, const String &p_url, const PackedStringArray &p_protocols, const PackedStringArray &p_headers) {
	if (sockets.count(p_id) || sockets.size() >= 128) {
		_emit("websocketFailed", p_id, folly::dynamic::object("message", "Socket ID is duplicate or the peer limit is reached"));
		return;
	}
	Socket socket;
	socket.peer = Ref<WebSocketPeer>(WebSocketPeer::create());
	socket.peer->set_inbound_buffer_size(settings.limit("network/websocket/max_buffered_bytes"));
	socket.peer->set_outbound_buffer_size(settings.limit("network/websocket/max_buffered_bytes"));
	socket.peer->set_max_queued_packets(settings.limit("network/websocket/max_queued_packets"));
	socket.peer->set_supported_protocols(p_protocols);
	socket.peer->set_handshake_headers(p_headers);
	const ::Error error = socket.peer->connect_to_url(p_url, TLSOptions::client());
	if (error != OK) {
		socket.failure = vformat("WebSocket connect failed (%d)", int(error));
		socket.peer->close(-1);
	}
	sockets.emplace(p_id, std::move(socket));
}

void RNWebSocketHub::send(int p_id, const PackedByteArray &p_bytes, bool p_text) {
	auto found = sockets.find(p_id);
	if (found == sockets.end() || found->second.peer->get_ready_state() != WebSocketPeer::STATE_OPEN || found->second.closing) {
		return;
	}
	Socket &socket = found->second;
	if (p_bytes.size() > settings.limit("network/websocket/max_message_bytes") || p_bytes.size() > settings.limit("network/websocket/max_buffered_bytes") - socket.peer->get_current_outbound_buffered_amount()) {
		socket.failure = "Socket outbound budget exceeded";
		socket.peer->close(-1);
		return;
	}
	socket.peer->send(p_bytes.ptr(), p_bytes.size(), p_text ? WebSocketPeer::WRITE_MODE_TEXT : WebSocketPeer::WRITE_MODE_BINARY);
}

void RNWebSocketHub::send_blob(int p_id, const Dictionary &p_blob) {
	PackedByteArray bytes;
	RNError error;
	if (!read_blob(blobs, p_blob, bytes, error)) {
		_emit("websocketFailed", p_id, folly::dynamic::object("message", error.message.utf8().get_data()));
		return;
	}
	send(p_id, bytes, false);
}

void RNWebSocketHub::close(int p_id, int p_code, const String &p_reason) {
	auto found = sockets.find(p_id);
	if (found == sockets.end() || found->second.closing) {
		return;
	}
	Socket &socket = found->second;
	socket.closing = true;
	socket.close_deadline = double(OS::get_singleton()->get_ticks_usec()) / 1000 + settings.limit("network/websocket/close_timeout_ms");
	socket.peer->close(p_code, p_reason);
}

void RNWebSocketHub::set_blob_messages(int p_id, bool p_enabled) {
	if (auto found = sockets.find(p_id); found != sockets.end()) {
		found->second.blob_messages = p_enabled;
	}
}

void RNWebSocketHub::process_frame() {
	std::vector<int> closed;
	for (auto &[id, socket] : sockets) {
		if (!socket.failure.is_empty()) {
			_emit("websocketFailed", id, folly::dynamic::object("message", socket.failure.utf8().get_data()));
			closed.push_back(id);
			continue;
		}
		socket.peer->poll();
		WebSocketPeer::State state = socket.peer->get_ready_state();
		if (state == WebSocketPeer::STATE_OPEN && !socket.opened && !socket.closing) {
			socket.opened = true;
			_emit("websocketOpen", id, folly::dynamic::object("protocol", socket.peer->get_selected_protocol().utf8().get_data()));
		}
		int packets = 0;
		while (socket.peer->get_available_packet_count() > 0 && packets++ < settings.limit("network/websocket/packets_per_frame")) {
			const uint8_t *data = nullptr;
			int size = 0;
			if (socket.peer->get_packet(&data, size) != OK || size > settings.limit("network/websocket/max_message_bytes")) {
				socket.failure = "Socket inbound message exceeds limit or failed";
				socket.peer->close(-1);
				break;
			}
			PackedByteArray packet;
			packet.resize(size);
			if (size) {
				memcpy(packet.ptrw(), data, size);
			}
			if (socket.peer->was_string_packet()) {
				_emit("websocketMessage", id, folly::dynamic::object("type", "text")("data", std::string(reinterpret_cast<const char *>(data), size)));
			} else if (socket.blob_messages) {
				RNError error;
				const Dictionary blob = blobs->store(packet, error);
				if (error.is_set()) {
					socket.failure = error.message;
					socket.peer->close(-1);
					break;
				}
				_emit("websocketMessage", id, folly::dynamic::object("type", "blob")("data", rn_dynamic_from_variant(blob)));
			} else {
				_emit("websocketMessage", id, folly::dynamic::object("type", "binary")("data", CryptoCore::b64_encode_str(packet.ptr(), packet.size()).utf8().get_data()));
			}
		}
		if (!socket.failure.is_empty()) {
			continue;
		}
		if (socket.closing && state != WebSocketPeer::STATE_CLOSED && double(OS::get_singleton()->get_ticks_usec()) / 1000 >= socket.close_deadline) {
			socket.forced = true;
			socket.peer->close(-1);
			state = WebSocketPeer::STATE_CLOSED;
		}
		if (state == WebSocketPeer::STATE_CLOSED) {
			if (!socket.opened && !socket.closing) {
				_emit("websocketFailed", id, folly::dynamic::object("message", "Socket handshake failed"));
			} else {
				const int code = socket.peer->get_close_code();
				_emit("websocketClosed", id, folly::dynamic::object("code", code > 0 ? code : 1006)("reason", socket.peer->get_close_reason().utf8().get_data()));
			}
			closed.push_back(id);
		}
	}
	for (int id : closed) {
		sockets.erase(id);
	}
}

void RNWebSocketHub::shutdown() {
	for (auto &[id, socket] : sockets) {
		socket.peer->close(-1);
	}
	sockets.clear();
}

RNBlobModule::RNBlobModule(std::shared_ptr<CallInvoker> jsInvoker, RNNetworkModuleContext p_context) :
		NativeBlobModuleCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
}

jsi::Object RNBlobModule::getConstants(jsi::Runtime &rt) {
	return jsi::valueFromDynamic(rt, folly::dynamic::object("BLOB_URI_SCHEME", "blob")("BLOB_URI_HOST", nullptr)).asObject(rt);
}

void RNBlobModule::addWebSocketHandler(jsi::Runtime &, double id) {
	context.main_thread->post([sockets = context.sockets, id = int(id)]() { sockets->set_blob_messages(id, true); });
}

void RNBlobModule::removeWebSocketHandler(jsi::Runtime &, double id) {
	context.main_thread->post([sockets = context.sockets, id = int(id)]() { sockets->set_blob_messages(id, false); });
}

void RNBlobModule::sendOverSocket(jsi::Runtime &rt, jsi::Object blob, double socketID) {
	const Dictionary descriptor = blob_descriptor(jsi::dynamicFromValue(rt, jsi::Value(rt, blob)));
	context.main_thread->post([sockets = context.sockets, descriptor, id = int(socketID)]() { sockets->send_blob(id, descriptor); });
}

void RNBlobModule::createFromParts(jsi::Runtime &rt, jsi::Array parts, const std::string &withId) {
	PackedByteArray bytes;
	RNError error;
	const folly::dynamic values = jsi::dynamicFromValue(rt, jsi::Value(rt, parts));
	for (const folly::dynamic &part : values) {
		PackedByteArray chunk;
		if (part["type"].asString() == "string") {
			chunk = utf8_bytes(part["data"].asString());
		} else if (!read_blob(context.blobs, blob_descriptor(part["data"]), chunk, error)) {
			rn_throw_js_error(rt, error.code.utf8().get_data(), error.message.utf8().get_data());
		}
		bytes.append_array(chunk);
	}
	if (!context.blobs->create(godot_string(withId), bytes, error)) {
		rn_throw_js_error(rt, error.code.utf8().get_data(), error.message.utf8().get_data());
	}
}

void RNBlobModule::release(jsi::Runtime &, const std::string &blobId) {
	context.blobs->release(godot_string(blobId));
}

RNFileReaderModule::RNFileReaderModule(std::shared_ptr<CallInvoker> jsInvoker, RNNetworkModuleContext p_context) :
		NativeFileReaderModuleCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
}

jsi::Value RNFileReaderModule::_read(jsi::Runtime &rt, jsi::Object data, bool p_as_url, const std::string &p_encoding) {
	const folly::dynamic blob = jsi::dynamicFromValue(rt, jsi::Value(rt, data));
	return RNJSPromise::create(rt, jsInvoker_, [this, blob, p_as_url, p_encoding](std::shared_ptr<RNJSPromise> p_promise) {
		const String encoding = godot_string(p_encoding).to_lower();
		if (!p_as_url && encoding != "utf-8" && encoding != "utf8") {
			p_promise->reject("E_UNSUPPORTED", "Only UTF-8 Blob text decoding is supported.");
			return;
		}
		PackedByteArray bytes;
		RNError error;
		if (!read_blob(context.blobs, blob_descriptor(blob), bytes, error)) {
			p_promise->reject(error.code.utf8().get_data(), error.message.utf8().get_data());
			return;
		}
		std::string result;
		if (p_as_url) {
			const std::string type = blob.getDefault("type", "").asString();
			result = "data:" + (type.empty() ? std::string("application/octet-stream") : type) + ";base64," + CryptoCore::b64_encode_str(bytes.ptr(), bytes.size()).utf8().get_data();
		} else {
			result = decoded_utf8(bytes);
		}
		p_promise->resolve([result](jsi::Runtime &p_runtime) { return jsi::String::createFromUtf8(p_runtime, result); });
	});
}

RNNetworkingModule::RNNetworkingModule(std::shared_ptr<CallInvoker> jsInvoker, RNNetworkModuleContext p_context) :
		NativeNetworkingIOSCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
}

void RNNetworkingModule::_emit(const char *p_name, folly::dynamic p_arguments) {
	emitDeviceEvent(p_name, [arguments = std::move(p_arguments)](jsi::Runtime &p_runtime, std::vector<jsi::Value> &r_arguments) {
		r_arguments.emplace_back(jsi::valueFromDynamic(p_runtime, arguments));
	});
}

void RNNetworkingModule::sendRequest(jsi::Runtime &rt, jsi::Object query, jsi::Function callback) {
	const int id = next_request++;
	const folly::dynamic request = jsi::dynamicFromValue(rt, jsi::Value(rt, query));
	const std::string response_type = request.getDefault("responseType", "text").asString();
	RNHTTPRequest native;
	RNError error;
	const bool built = rn_build_http_request(http_record(request), context.blobs, context.http->upload_limit(), "godot-" + itos(id), native, error);
	callback.call(rt, id);
	if (!built) {
		_emit("didCompleteNetworkResponse", folly::dynamic::array(id, error.message.utf8().get_data(), false));
		return;
	}
	auto respond = [this, id, response_type, requests = requests, mutex = mutex](RNHTTPResponse p_response) {
		{
			std::lock_guard lock(*mutex);
			requests->erase(id);
		}
		if (p_response.error.is_set()) {
			_emit("didCompleteNetworkResponse", folly::dynamic::array(id, p_response.error.message.utf8().get_data(), p_response.error.code == "E_TIMEOUT"));
			return;
		}
		folly::dynamic headers = folly::dynamic::object();
		std::map<std::string, std::string> spelling;
		std::string content_type;
		for (const String &line : p_response.headers) {
			const int colon = line.find(":");
			if (colon <= 0) {
				continue;
			}
			const std::string name = line.substr(0, colon).strip_edges().utf8().get_data();
			const std::string value = line.substr(colon + 1).strip_edges().utf8().get_data();
			const std::string lower = line.substr(0, colon).strip_edges().to_lower().utf8().get_data();
			const std::string key = spelling.emplace(lower, name).first->second;
			headers[key] = headers.count(key) ? headers[key].asString() + ", " + value : value;
			if (lower == "content-type") {
				content_type = value;
			}
		}
		_emit("didReceiveNetworkResponse", folly::dynamic::array(id, p_response.status, headers, p_response.url.utf8().get_data()));
		folly::dynamic data;
		if (response_type == "blob") {
			RNError storage_error;
			Dictionary blob = context.blobs->store(p_response.body, storage_error);
			if (storage_error.is_set()) {
				_emit("didCompleteNetworkResponse", folly::dynamic::array(id, storage_error.message.utf8().get_data(), false));
				return;
			}
			blob["type"] = godot_string(content_type);
			data = rn_dynamic_from_variant(blob);
		} else if (response_type == "base64") {
			data = CryptoCore::b64_encode_str(p_response.body.ptr(), p_response.body.size()).utf8().get_data();
		} else {
			data = decoded_utf8(p_response.body);
		}
		_emit("didReceiveNetworkData", folly::dynamic::array(id, data));
		_emit("didCompleteNetworkResponse", folly::dynamic::array(id, "", false));
	};
	context.main_thread->post([http = context.http, blobs = context.blobs, native, respond, id, requests = requests, mutex = mutex]() {
		if (native.url.begins_with("blob:")) {
			RNHTTPResponse response;
			response.url = native.url;
			response.status = 200;
			read_blob(blobs, blob_url_descriptor(native.url), response.body, response.error);
			respond(response);
			return;
		}
		RNError submit_error;
		const uint64_t native_id = http->submit(native, respond, submit_error);
		if (submit_error.is_set()) {
			RNHTTPResponse failed;
			failed.error = submit_error;
			respond(failed);
			return;
		}
		std::lock_guard lock(*mutex);
		(*requests)[id] = native_id;
	});
}

void RNNetworkingModule::abortRequest(jsi::Runtime &, double requestId) {
	context.main_thread->post([http = context.http, requests = requests, mutex = mutex, id = int(requestId)]() {
		uint64_t native_id = 0;
		{
			std::lock_guard lock(*mutex);
			auto found = requests->find(id);
			if (found == requests->end()) {
				return;
			}
			native_id = found->second;
			requests->erase(found);
		}
		http->cancel(native_id);
	});
}

void RNNetworkingModule::clearCookies(jsi::Runtime &rt, jsi::Function callback) {
	auto pending = std::make_shared<jsi::Function>(std::move(callback));
	context.main_thread->post([http = context.http, pending, invoker = jsInvoker_]() {
		const bool cleared = http->clear_cookies();
		invoker->invokeAsync([pending, cleared](jsi::Runtime &p_runtime) { pending->call(p_runtime, cleared); });
	});
}

RNWebSocketModule::RNWebSocketModule(std::shared_ptr<CallInvoker> jsInvoker, RNNetworkModuleContext p_context) :
		NativeWebSocketModuleCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
}

void RNWebSocketModule::connect(jsi::Runtime &rt, const std::string &url, std::optional<jsi::Array> protocols, jsi::Object options, double socketID) {
	PackedStringArray selected;
	if (protocols) {
		for (size_t i = 0; i < protocols->size(rt); ++i) {
			const String protocol = godot_string(protocols->getValueAtIndex(rt, i).asString(rt).utf8(rt));
			if (protocol.is_empty() || protocol.contains(",") || protocol.contains("\r") || protocol.contains("\n") || selected.has(protocol)) {
				rn_throw_js_error(rt, "E_VALIDATION", "Invalid socket subprotocol");
			}
			selected.push_back(protocol);
		}
	}
	PackedStringArray headers;
	const folly::dynamic settings = jsi::dynamicFromValue(rt, jsi::Value(rt, options));
	if (const folly::dynamic *values = settings.get_ptr("headers"); values && values->isObject()) {
		for (const auto &[name, value] : values->items()) {
			const String header = godot_string(name.asString()) + ": " + godot_string(value.asString());
			if (godot_string(name.asString()).contains(":") || header.contains("\r") || header.contains("\n")) {
				rn_throw_js_error(rt, "E_VALIDATION", "Invalid handshake header");
			}
			headers.push_back(header);
		}
	}
	const String target = godot_string(url);
	if (!target.begins_with("ws://") && !target.begins_with("wss://")) {
		rn_throw_js_error(rt, "E_VALIDATION", "Socket URL must use ws or wss");
	}
	context.main_thread->post([sockets = context.sockets, id = int(socketID), target, selected, headers]() { sockets->connect(id, target, selected, headers); });
}

void RNWebSocketModule::send(jsi::Runtime &, const std::string &message, double forSocketID) {
	context.main_thread->post([sockets = context.sockets, bytes = utf8_bytes(message), id = int(forSocketID)]() { sockets->send(id, bytes, true); });
}

void RNWebSocketModule::sendBinary(jsi::Runtime &, const std::string &base64String, double forSocketID) {
	context.main_thread->post([sockets = context.sockets, bytes = base64_bytes(base64String), id = int(forSocketID)]() { sockets->send(id, bytes, false); });
}

void RNWebSocketModule::ping(jsi::Runtime &rt, double) {
	rn_throw_js_error(rt, "E_UNSUPPORTED", "Godot WebSocketPeer does not expose ping frames.");
}

void RNWebSocketModule::close(jsi::Runtime &rt, double code, const std::string &reason, double socketID) {
	const int close_code = int(code);
	if ((close_code != 1000 && (close_code < 3000 || close_code > 4999)) || reason.size() > 123) {
		rn_throw_js_error(rt, "E_VALIDATION", "Invalid WebSocket close code or reason");
	}
	context.main_thread->post([sockets = context.sockets, id = int(socketID), close_code, reason = godot_string(reason)]() { sockets->close(id, close_code, reason); });
}

} // namespace facebook::react
