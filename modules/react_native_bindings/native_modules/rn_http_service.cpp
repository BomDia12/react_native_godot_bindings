#include "rn_http_service.h"

#include "../interop/rn_resource_path.h"
#include "rn_blob_service.h"

#include "core/crypto/crypto.h"
#include "core/io/file_access.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/os/time.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
RNError http_error(const String &p_message, const String &p_code = RNErrorCode::NATIVE) {
	return RNError::make(p_code, p_message, "HTTP");
}
String header_value(const PackedStringArray &p_headers, const String &p_name) {
	for (const auto &header : p_headers) {
		const int colon = header.find(":");
		if (colon >= 0 && header.substr(0, colon).to_lower() == p_name) {
			return header.substr(colon + 1).strip_edges();
		}
	}
	return String();
}
HTTPClient::Method method_of(const String &p_name) {
	for (int method = 0; method < HTTPClient::METHOD_MAX; ++method) {
		if (p_name == String(PackedStringArray({ "GET", "HEAD", "POST", "PUT", "DELETE", "OPTIONS", "TRACE", "CONNECT", "PATCH" })[method])) {
			return HTTPClient::Method(method);
		}
	}
	return HTTPClient::METHOD_MAX;
}
bool header_valid(const String &p_header) {
	const int colon = p_header.find(":");
	if (colon <= 0 || p_header.contains("\r") || p_header.contains("\n")) {
		return false;
	}
	const String name = p_header.substr(0, colon);
	for (int i = 0; i < name.length(); ++i) {
		const char32_t value = name[i];
		if (value <= 32 || value >= 127 || String("()<>@,;:\\[]?={}\"").contains(String::chr(value))) {
			return false;
		}
	}
	return true;
}
} //namespace
double RNHTTPService::now() {
	return double(OS::get_singleton()->get_ticks_usec()) / 1000;
}
RNHTTPService::RNHTTPService(const RNServiceSettings &p_settings) :
		settings(p_settings), cookies(p_settings.limit("network/cookies/max_entries"), p_settings.limit("network/cookies/max_bytes")) {
	set_process_mode(Node::PROCESS_MODE_ALWAYS);
}
RNHTTPService::~RNHTTPService() {
	shutdown();
}
void RNHTTPService::_bind_methods() {}
bool RNHTTPService::set_trust_resource(const String &p_path, RNError &r_error) {
	if (p_path.is_empty()) {
		tls_options = TLSOptions::client();
		return true;
	}
	String normalized;
	if (!rn_normalize_local_resource_path(p_path, normalized, r_error)) {
		return false;
	}
	Ref<X509Certificate> certificate = Ref<X509Certificate>(X509Certificate::create());
	if (certificate.is_null() || certificate->load(normalized) != OK) {
		r_error = http_error("TLS trust resource failed to load", RNErrorCode::VALIDATION);
		return false;
	}
	tls_options = TLSOptions::client(certificate);
	return true;
}
uint64_t RNHTTPService::submit(const RNHTTPRequest &p_request, std::function<void(RNHTTPResponse)> p_completion, RNError &r_error) {
	RNParsedURL url;
	if (closed || !is_inside_tree()) {
		r_error = http_error("HTTP service is closed");
		return 0;
	}
	if (!RNParsedURL::parse(p_request.url, url) || (url.scheme != "http" && url.scheme != "https") || method_of(p_request.method) == HTTPClient::METHOD_MAX || !std::isfinite(p_request.timeout_ms) || p_request.timeout_ms < 0) {
		r_error = http_error("Invalid HTTP URL, method or timeout", RNErrorCode::VALIDATION);
		return 0;
	}
	for (const auto &header : p_request.headers) {
		if (!header_valid(header)) {
			r_error = http_error("Invalid HTTP header", RNErrorCode::VALIDATION);
			return 0;
		}
	}
	if (requests.size() >= uint64_t(settings.limit("network/http/max_active_requests") + settings.limit("network/http/max_queued_requests")) || uint64_t(p_request.body.size()) > uint64_t(settings.limit("network/http/max_buffered_bytes")) - buffered) {
		r_error = http_error("HTTP queue or upload budget exceeded", RNErrorCode::LIMIT);
		return 0;
	}
	const uint64_t response_limit = p_request.response_limit ? std::min(p_request.response_limit, uint64_t(settings.limit("network/http/max_body_bytes"))) : settings.limit("network/http/max_body_bytes");
	if (uint64_t(p_request.body.size()) + response_limit > uint64_t(settings.limit("network/http/max_buffered_bytes"))) {
		r_error = http_error("Upload and response reservation exceed the buffer ceiling", RNErrorCode::LIMIT);
		return 0;
	}
	bool available_slot = slots.size() < uint64_t(settings.limit("network/http/max_active_requests"));
	for (const Slot &slot : slots) {
		available_slot = available_slot || slot.state == State::IDLE;
	}
	const bool immediate = waiting.empty() && available_slot && uint64_t(p_request.body.size()) + response_limit <= uint64_t(settings.limit("network/http/max_buffered_bytes")) - buffered;
	if (!immediate && waiting.size() >= uint64_t(settings.limit("network/http/max_queued_requests"))) {
		r_error = http_error("HTTP FIFO is full", RNErrorCode::LIMIT);
		return 0;
	}
	Request request;
	request.id = next_id++;
	request.spec = p_request;
	request.spec.response_limit = p_request.response_limit ? std::min(p_request.response_limit, uint64_t(settings.limit("network/http/max_body_bytes"))) : settings.limit("network/http/max_body_bytes");
	request.deadline = p_request.timeout_ms > 0 ? now() + p_request.timeout_ms : 0;
	request.completion = std::move(p_completion);
	buffered += p_request.body.size();
	const uint64_t id = request.id;
	requests.emplace(id, std::move(request));
	waiting.push_back(id);
	process_requests();
	return id;
}
void RNHTTPService::settle(uint64_t p_request, RNHTTPResponse p_response) {
	auto found = requests.find(p_request);
	if (found == requests.end()) {
		return;
	}
	auto callback = std::move(found->second.completion);
	buffered -= found->second.spec.body.size();
	requests.erase(found);
	if (callback) {
		callback(std::move(p_response));
	}
}
void RNHTTPService::quarantine(size_t p_slot) {
	Slot &slot = slots[p_slot];
	if (slot.state == State::DRAINING) {
		return;
	}
	slot.state = State::DRAINING;
	HTTPRequest *node = Object::cast_to<HTTPRequest>(ObjectDB::get_instance(slot.node));
	if (node) {
		node->cancel_request();
	}
	callable_mp(this, &RNHTTPService::_drained).call_deferred(int(p_slot), int64_t(slot.sequence));
}
void RNHTTPService::_drained(int p_slot, int64_t p_sequence) {
	if (p_slot < 0 || size_t(p_slot) >= slots.size()) {
		return;
	}
	Slot &slot = slots[p_slot];
	if (slot.sequence != uint64_t(p_sequence) || slot.state != State::DRAINING) {
		return;
	}
	HTTPRequest *node = Object::cast_to<HTTPRequest>(ObjectDB::get_instance(slot.node));
	if (node && node->is_connected("request_completed", slot.callback)) {
		node->disconnect("request_completed", slot.callback);
	}
	slot.callback = Callable();
	buffered -= slot.reservation;
	slot.reservation = 0;
	slot.state = State::IDLE;
	const uint64_t request_id = slot.request;
	slot.request = 0;
	auto request = requests.find(request_id);
	if (request != requests.end() && request->second.cancelled) {
		RNHTTPResponse response;
		response.error = http_error("HTTP request cancelled", RNErrorCode::CANCELLED);
		settle(request_id, std::move(response));
	}
	size_t idle = 0;
	for (const auto &entry : slots) {
		if (entry.state == State::IDLE && entry.node.is_valid()) {
			++idle;
		}
	}
	if (node && idle > uint64_t(settings.limit("network/http/max_idle_requests"))) {
		remove_child(node);
		memdelete(node);
		slots[p_slot].node = ObjectID();
	}
	if (!closed) {
		process_requests();
	}
}
bool RNHTTPService::cancel(uint64_t p_request) {
	auto found = requests.find(p_request);
	if (found == requests.end()) {
		return true;
	}
	for (size_t index = 0; index < slots.size(); ++index) {
		if (slots[index].request == p_request && slots[index].state != State::IDLE) {
			found->second.cancelled = true;
			quarantine(index);
			return false;
		}
	}
	buffered -= found->second.spec.body.size();
	requests.erase(found);
	waiting.erase(std::remove(waiting.begin(), waiting.end(), p_request), waiting.end());
	return true;
}
void RNHTTPService::lease(size_t p_slot, uint64_t p_request) {
	Slot &slot = slots[p_slot];
	Request &request = requests.at(p_request);
	HTTPRequest *node = Object::cast_to<HTTPRequest>(ObjectDB::get_instance(slot.node));
	if (!node) {
		node = memnew(HTTPRequest);
		node->set_use_threads(true);
		node->set_process_mode(Node::PROCESS_MODE_ALWAYS);
		add_child(node);
		slot.node = node->get_instance_id();
	}
	slot.state = State::LEASED;
	slot.request = p_request;
	++slot.sequence;
	slot.reservation = request.spec.response_limit;
	buffered += slot.reservation;
	++started;
	slot.callback = callable_mp(this, &RNHTTPService::_completed).bind(int(p_slot), int64_t(slot.sequence));
	node->connect("request_completed", slot.callback);
	node->set_max_redirects(0);
	node->set_body_size_limit(request.spec.response_limit);
	node->set_timeout(request.deadline > 0 ? std::max(0.001, (request.deadline - now()) / 1000) : 0);
	node->set_tls_options(tls_options.is_valid() ? tls_options : TLSOptions::client());
	PackedStringArray headers = request.spec.headers;
	if (request.spec.credentials) {
		const String cookie = cookies.header(request.spec.url, Time::get_singleton()->get_unix_time_from_system());
		if (!cookie.is_empty() && header_value(headers, "cookie").is_empty()) {
			headers.push_back("Cookie: " + cookie);
		}
	}
	const Error error = node->request_raw(request.spec.url, headers, method_of(request.spec.method), request.spec.body);
	if (error != OK) {
		RNHTTPResponse response;
		response.error = http_error(vformat("HTTPRequest failed to start (%d)", error));
		quarantine(p_slot);
		settle(p_request, std::move(response));
	}
}
void RNHTTPService::process_requests() {
	if (closed) {
		return;
	}
	std::vector<uint64_t> expired;
	for (const auto &entry : requests) {
		if (entry.second.deadline > 0 && now() >= entry.second.deadline && !entry.second.cancelled) {
			expired.push_back(entry.first);
		}
	}
	for (uint64_t id : expired) {
		auto found = requests.find(id);
		if (found == requests.end()) {
			continue;
		}
		RNHTTPResponse response;
		response.error = http_error("HTTP request timed out", "E_TIMEOUT");
		for (size_t index = 0; index < slots.size(); ++index) {
			if (slots[index].request == id) {
				quarantine(index);
				break;
			}
		}
		settle(id, std::move(response));
	}
	while (!waiting.empty()) {
		const uint64_t id = waiting.front();
		auto found = requests.find(id);
		if (found == requests.end()) {
			waiting.pop_front();
			continue;
		}
		bool draining = false;
		for (const Slot &slot : slots) {
			draining = draining || (slot.state != State::IDLE && slot.request == id);
		}
		if (draining) {
			break;
		}
		if (found->second.spec.response_limit > uint64_t(settings.limit("network/http/max_buffered_bytes")) - buffered) {
			break;
		}
		size_t index = 0;
		while (index < slots.size() && slots[index].state != State::IDLE) {
			++index;
		}
		if (index == slots.size()) {
			if (slots.size() >= uint64_t(settings.limit("network/http/max_active_requests"))) {
				break;
			}
			slots.emplace_back();
		}
		waiting.pop_front();
		lease(index, id);
	}
}
void RNHTTPService::_completed(int p_result, int p_status, const PackedStringArray &p_headers, const PackedByteArray &p_body, int p_slot, int64_t p_sequence) {
	if (p_slot < 0 || size_t(p_slot) >= slots.size()) {
		return;
	}
	Slot &slot = slots[p_slot];
	if (slot.sequence != uint64_t(p_sequence) || slot.state != State::LEASED) {
		return;
	}
	auto found = requests.find(slot.request);
	if (found == requests.end()) {
		quarantine(p_slot);
		return;
	}
	Request &request = found->second;
	if (request.spec.credentials) {
		cookies.receive(request.spec.url, p_headers, Time::get_singleton()->get_unix_time_from_system());
	}
	RNHTTPResponse response;
	response.url = request.spec.url;
	response.status = p_status;
	response.headers = p_headers;
	const String location = header_value(p_headers, "location");
	if ((p_status == 301 || p_status == 302 || p_status == 303 || p_status == 307 || p_status == 308) && !location.is_empty() && (p_result == HTTPRequest::RESULT_REDIRECT_LIMIT_REACHED || p_result == HTTPRequest::RESULT_SUCCESS)) {
		if (request.redirects >= settings.limit("network/http/max_redirects")) {
			response.error = http_error("HTTP redirect limit exceeded");
		} else {
			RNParsedURL old_url;
			RNParsedURL::parse(request.spec.url, old_url);
			String next;
			RNParsedURL new_url;
			if (!RNParsedURL::resolve(request.spec.url, location, next) || !RNParsedURL::parse(next, new_url) || (new_url.scheme != "http" && new_url.scheme != "https")) {
				response.error = http_error("Invalid redirect URL");
			} else {
				PackedStringArray headers;
				for (const auto &header : request.spec.headers) {
					const String name = header.get_slice(":", 0).to_lower();
					if (old_url.origin() != new_url.origin() && (name == "authorization" || name == "cookie" || name == "proxy-authorization")) {
						continue;
					}
					headers.push_back(header);
				}
				request.spec.headers = headers;
				if ((p_status == 303 && request.spec.method != "HEAD") || ((p_status == 301 || p_status == 302) && request.spec.method == "POST")) {
					request.spec.method = "GET";
					buffered -= request.spec.body.size();
					request.spec.body.clear();
					PackedStringArray retained;
					for (const auto &header : request.spec.headers) {
						const String name = header.get_slice(":", 0).to_lower();
						if (name != "content-type" && name != "content-length") {
							retained.push_back(header);
						}
					}
					request.spec.headers = retained;
				}
				request.spec.url = next;
				++request.redirects;
				waiting.push_front(request.id);
				quarantine(p_slot);
				return;
			}
		}
	} else if (p_result != HTTPRequest::RESULT_SUCCESS) {
		response.error = http_error(vformat("HTTPRequest transport result %d", p_result), p_result == HTTPRequest::RESULT_TIMEOUT ? String("E_TIMEOUT") : p_result == HTTPRequest::RESULT_BODY_SIZE_LIMIT_EXCEEDED ? String(RNErrorCode::LIMIT)
																																																				   : String(RNErrorCode::NATIVE));
	} else if (uint64_t(p_body.size()) > request.spec.response_limit) {
		response.error = http_error("HTTP body exceeds configured limit", RNErrorCode::LIMIT);
	} else {
		response.body = p_body;
	}
	const uint64_t id = slot.request;
	quarantine(p_slot);
	settle(id, std::move(response));
}
bool RNHTTPService::has_pending_work() const {
	if (!requests.empty()) {
		return true;
	}
	for (const auto &slot : slots) {
		if (slot.state != State::IDLE) {
			return true;
		}
	}
	return false;
}
void RNHTTPService::shutdown() {
	if (closed) {
		return;
	}
	closed = true;
	for (auto &slot : slots) {
		auto node = Object::cast_to<HTTPRequest>(ObjectDB::get_instance(slot.node));
		if (node) {
			node->cancel_request();
			if (node->is_connected("request_completed", slot.callback)) {
				node->disconnect("request_completed", slot.callback);
			}
			remove_child(node);
			memdelete(node);
		}
		slot.node = ObjectID();
		slot.callback = Callable();
		slot.state = State::IDLE;
	}
	requests.clear();
	waiting.clear();
	buffered = 0;
	cookies.clear();
}
Dictionary RNHTTPService::stats() const {
	Dictionary value;
	int active = 0, idle = 0, draining = 0;
	for (const auto &slot : slots) {
		if (slot.state == State::LEASED) {
			++active;
		} else if (slot.state == State::DRAINING) {
			++draining;
		} else if (slot.node.is_valid()) {
			++idle;
		}
	}
	value["active"] = active;
	value["idle"] = idle;
	value["draining"] = draining;
	value["queued"] = int(waiting.size());
	value["bufferedBytes"] = int64_t(buffered);
	value["started"] = int64_t(started);
	value["cookies"] = int(cookies.count());
	return value;
}

namespace {
class HTTPImageTransport : public RNImageTransport {
	std::weak_ptr<RNHTTPService> service;
	ObjectID service_id;

public:
	explicit HTTPImageTransport(const std::shared_ptr<RNHTTPService> &p_service) :
			service(p_service), service_id(p_service->get_instance_id()) {}
	uint64_t start(const RNImageSource &p_source, uint64_t p_limit, std::function<void(RNImageTransportResponse)> p_completion) override {
		auto owner = service.lock();
		if (!owner || !ObjectDB::get_instance(service_id)) {
			RNImageTransportResponse response;
			response.error = http_error("Image transport closed");
			p_completion(std::move(response));
			return 0;
		}
		RNHTTPRequest request;
		request.url = p_source.uri;
		request.response_limit = p_limit;
		request.credentials = !p_source.stateless;
		for (const Variant &key : p_source.headers.keys()) {
			request.headers.push_back(String(key) + ": " + String(p_source.headers[key]));
		}
		RNError error;
		const uint64_t id = owner->submit(request, [p_completion](RNHTTPResponse p_response) {RNImageTransportResponse response;response.bytes=p_response.body;response.error=p_response.error;if(!response.error.is_set() && (p_response.status<200 || p_response.status>=300)){response.error=http_error("Image HTTP status is not successful");}const String cache=header_value(p_response.headers,"cache-control").to_lower();response.cacheable=!cache.contains("no-store") && !cache.contains("no-cache");p_completion(std::move(response)); }, error);
		if (error.is_set()) {
			RNImageTransportResponse response;
			response.error = error;
			p_completion(std::move(response));
		}
		return id;
	}
	bool cancel(uint64_t p_request) override {
		auto owner = service.lock();
		return !owner || !ObjectDB::get_instance(service_id) || owner->cancel(p_request);
	}
};
} //namespace
std::shared_ptr<RNImageTransport> rn_http_image_transport(const std::shared_ptr<RNHTTPService> &p_service) {
	return std::make_shared<HTTPImageTransport>(p_service);
}

namespace {
bool append_bytes(PackedByteArray &r_body, const PackedByteArray &p_bytes, uint64_t p_limit, RNError &r_error) {
	if (uint64_t(p_bytes.size()) > p_limit - uint64_t(r_body.size())) {
		r_error = http_error("Upload exceeds configured buffer limit", RNErrorCode::LIMIT);
		return false;
	}
	const int64_t offset = r_body.size();
	r_body.resize(offset + p_bytes.size());
	if (p_bytes.size()) {
		std::memcpy(r_body.ptrw() + offset, p_bytes.ptr(), p_bytes.size());
	}
	return true;
}
bool append_blob(PackedByteArray &r_body, const Dictionary &p_data, const std::shared_ptr<RNBlobService> &p_blobs, uint64_t p_limit, RNError &r_error) {
	if (!p_blobs->pin(p_data, r_error)) {
		return false;
	}
	const String id = p_data["blobId"];
	struct Guard {
		std::shared_ptr<RNBlobService> blobs;
		String id;
		~Guard() { blobs->unpin(id); }
	} guard{ p_blobs, id };
	const int64_t size = p_data.get("size", 0);
	for (int64_t offset = 0; offset < size; offset += 1024 * 1024) {
		PackedByteArray bytes;
		if (!p_blobs->read(p_data, offset, std::min<int64_t>(1024 * 1024, size - offset), bytes, r_error) || !append_bytes(r_body, bytes, p_limit, r_error)) {
			return false;
		}
	}
	return true;
}
class RNHTTPModule : public RNNativeModule {
	std::shared_ptr<int> lifetime = std::make_shared<int>(0);
	std::function<std::shared_ptr<RNHTTPService>()> service;
	std::function<std::shared_ptr<RNBlobService>()> blobs;
	std::map<String, uint64_t> owned;
	std::map<String, String> completed_blobs;

public:
	RNHTTPModule(std::function<std::shared_ptr<RNHTTPService>()> p_service, std::function<std::shared_ptr<RNBlobService>()> p_blobs) :
			service(std::move(p_service)), blobs(std::move(p_blobs)) {}
	RNModuleResult invoke_sync(const StringName &p_method, const Array &p_args, const RNCallContext &) override {
		auto owner = service();
		if (!owner) {
			return RNModuleResult::failure(http_error("HTTP service unavailable"));
		}
		if (p_method == "stats") {
			return RNModuleResult::success(owner->stats());
		}
		if (p_method == "setTrustResource") {
			RNError error;
			owner->set_trust_resource(p_args[0], error);
			return error.is_set() ? RNModuleResult::failure(error) : RNModuleResult::success();
		}
		if (p_method == "clearCookies") {
			return RNModuleResult::success(owner->clear_cookies());
		}
		return RNModuleResult::failure(http_error("Unknown HTTP method"));
	}
	void start_async(const StringName &, const Array &p_args, const RNCallContext &p_context, const RNCompletionToken &p_completion) override {
		auto owner = service();
		auto binary = blobs();
		if (!owner || !binary) {
			p_completion.fail(http_error("HTTP service unavailable"));
			return;
		}
		if (p_args[0].get_type() != Variant::DICTIONARY) {
			p_completion.fail(http_error("HTTP request requires a record", RNErrorCode::VALIDATION));
			return;
		}
		const Dictionary data = p_args[0];
		if (data.get("url", Variant()).get_type() != Variant::STRING || data.get("method", Variant()).get_type() != Variant::STRING || data.get("credentials", Variant()).get_type() != Variant::BOOL || (data.get("timeout", Variant()).get_type() != Variant::FLOAT && data.get("timeout", Variant()).get_type() != Variant::INT)) {
			p_completion.fail(http_error("Invalid HTTP request fields", RNErrorCode::VALIDATION));
			return;
		}
		RNHTTPRequest request;
		request.url = data.get("url", String());
		request.method = String(data.get("method", "GET")).to_upper();
		request.timeout_ms = data.get("timeout", 0.0);
		request.credentials = data.get("credentials", true);
		if (data.get("headers", Variant()).get_type() != Variant::DICTIONARY) {
			p_completion.fail(http_error("HTTP headers require a record", RNErrorCode::VALIDATION));
			return;
		}
		const Dictionary headers = data["headers"];
		for (const Variant &key : headers.keys()) {
			if (headers[key].get_type() != Variant::STRING) {
				p_completion.fail(http_error("HTTP header values must be strings", RNErrorCode::VALIDATION));
				return;
			}
			request.headers.push_back(String(key) + ": " + String(headers[key]));
		}
		RNError error;
		const uint64_t upload_limit = owner->upload_limit();
		const Variant body = data.get("body", Variant());
		if (body.get_type() == Variant::PACKED_BYTE_ARRAY) {
			request.body = body;
		} else if (body.get_type() == Variant::DICTIONARY) {
			const Dictionary descriptor = body;
			bool loaded = false;
			if (descriptor.has("uri") && descriptor["uri"].get_type() == Variant::STRING) {
				String path;
				if (rn_normalize_local_resource_path(descriptor["uri"], path, error)) {
					Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
					if (file.is_valid() && file->get_length() <= upload_limit) {
						request.body = file->get_buffer(file->get_length());
						loaded = true;
					} else {
						error = http_error("Upload file is unavailable or too large", RNErrorCode::LIMIT);
					}
				}
			} else {
				loaded = append_blob(request.body, body, binary, upload_limit, error);
			}
			if (!loaded) {
				p_completion.fail(error);
				return;
			}
		} else if (body.get_type() != Variant::NIL) {
			p_completion.fail(http_error("HTTP body must be bytes or a Blob descriptor", RNErrorCode::VALIDATION));
			return;
		}
		if (data.has("formData")) {
			if (data["formData"].get_type() != Variant::ARRAY) {
				p_completion.fail(http_error("Multipart requires an array", RNErrorCode::VALIDATION));
				return;
			}
			const String boundary = "godot-" + p_context.request_token;
			const Array parts = data["formData"];
			for (const Variant &part_value : parts) {
				if (part_value.get_type() != Variant::DICTIONARY) {
					p_completion.fail(http_error("Invalid multipart part", RNErrorCode::VALIDATION));
					return;
				}
				const Dictionary part = part_value;
				if (part.get("headers", Variant()).get_type() != Variant::DICTIONARY) {
					p_completion.fail(http_error("Multipart headers require a record", RNErrorCode::VALIDATION));
					return;
				}
				const Dictionary part_headers = part["headers"];
				String prefix = "--" + boundary + "\r\n";
				for (const Variant &key : part_headers.keys()) {
					if (key.get_type() != Variant::STRING || part_headers[key].get_type() != Variant::STRING) {
						p_completion.fail(http_error("Multipart header names and values must be strings", RNErrorCode::VALIDATION));
						return;
					}
					const String header = String(key) + ": " + String(part_headers[key]);
					if (!header_valid(header)) {
						p_completion.fail(http_error("Invalid multipart header", RNErrorCode::VALIDATION));
						return;
					}
					prefix += header + "\r\n";
				}
				prefix += "\r\n";
				if (!append_bytes(request.body, prefix.to_utf8_buffer(), upload_limit, error)) {
					p_completion.fail(error);
					return;
				}
				if (part.has("blob")) {
					if (!append_blob(request.body, part["blob"], binary, upload_limit, error)) {
						p_completion.fail(error);
						return;
					}
				} else if (part.has("uri")) {
					String path;
					if (!rn_normalize_local_resource_path(part["uri"], path, error)) {
						p_completion.fail(error);
						return;
					}
					Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
					if (file.is_null() || file->get_length() > upload_limit) {
						p_completion.fail(http_error("Multipart file is unavailable or exceeds limit", RNErrorCode::LIMIT));
						return;
					}
					while (file->get_position() < file->get_length()) {
						if (!append_bytes(request.body, file->get_buffer(std::min<uint64_t>(1024 * 1024, file->get_length() - file->get_position())), upload_limit, error)) {
							p_completion.fail(error);
							return;
						}
					}
				} else if (part.has("bytes")) {
					if (!append_bytes(request.body, part["bytes"], upload_limit, error)) {
						p_completion.fail(error);
						return;
					}
				} else {
					p_completion.fail(http_error("Multipart part has no body", RNErrorCode::VALIDATION));
					return;
				}
				if (!append_bytes(request.body, String("\r\n").to_utf8_buffer(), upload_limit, error)) {
					p_completion.fail(error);
					return;
				}
			}
			if (!append_bytes(request.body, ("--" + boundary + "--\r\n").to_utf8_buffer(), upload_limit, error)) {
				p_completion.fail(error);
				return;
			}
			PackedStringArray retained;
			for (const auto &header : request.headers) {
				if (header.get_slice(":", 0).to_lower() != "content-type") {
					retained.push_back(header);
				}
			}
			retained.push_back("Content-Type: multipart/form-data; boundary=" + boundary);
			request.headers = retained;
		}
		auto completed = [this, lifetime_guard = std::weak_ptr<int>(lifetime), p_completion, binary, token = p_context.request_token](RNHTTPResponse response) {
			if (lifetime_guard.expired()) { return; }
			owned.erase(token);
			if (response.error.is_set()) {
				p_completion.fail(response.error);
				return;
			}
			RNError storage_error;
			Dictionary value;
			value["url"] = response.url;
			value["status"] = response.status;
			Array response_headers;
			for (const String &header : response.headers) {
				const int colon = header.find(":");
				if (colon > 0) {
					Array pair;
					pair.push_back(header.substr(0, colon));
					pair.push_back(header.substr(colon + 1).strip_edges());
					response_headers.push_back(pair);
				}
			}
			value["headers"] = response_headers;
			const Dictionary body_data = binary->store(response.body, storage_error);
			value["body"] = body_data;
			if (storage_error.is_set()) {
				p_completion.fail(storage_error);
			} else {
				completed_blobs[token] = body_data["blobId"];
				p_completion.complete(value);
			} };
		if (request.url.begins_with("blob:")) {
			Dictionary descriptor;
			RNHTTPResponse response;
			response.url = request.url;
			response.status = 200;
			if (!binary->resolve_url(request.url, descriptor, error) || !append_blob(response.body, descriptor, binary, owner->upload_limit(), error)) {
				p_completion.fail(error);
				return;
			}
			completed(std::move(response));
			return;
		}
		const uint64_t id = owner->submit(request, std::move(completed), error);
		if (error.is_set()) {
			p_completion.fail(error);
		} else {
			owned[p_context.request_token] = id;
		}
	}
	void cancel(const String &p_token) override {
		auto completed = completed_blobs.find(p_token);
		if (completed != completed_blobs.end()) {
			if (auto binary = blobs()) {
				binary->release(completed->second);
			}
			completed_blobs.erase(completed);
		}
		auto found = owned.find(p_token);
		if (found != owned.end()) {
			if (auto owner = service()) {
				owner->cancel(found->second);
			}
			owned.erase(found);
		}
	}
	void on_result_delivered(const String &p_token, bool p_success) override {
		auto found = completed_blobs.find(p_token);
		if (found != completed_blobs.end()) {
			if (!p_success) {
				if (auto binary = blobs()) {
					binary->release(found->second);
				}
			}
			completed_blobs.erase(found);
		}
	}
	void shutdown() override {
		lifetime.reset();
		if (auto binary = blobs()) {
			for (const auto &entry : completed_blobs) {
				binary->release(entry.second);
			}
		}
		completed_blobs.clear();
		auto requests = owned;
		for (const auto &entry : requests) {
			cancel(entry.first);
		}
	}
};
} //namespace
bool rn_register_http_module(RNNativeModuleRegistry &p_registry, const std::function<std::shared_ptr<RNHTTPService>()> &p_service, const std::function<std::shared_ptr<RNBlobService>()> &p_blobs, RNError &r_error) {
	RNModuleDefinition definition;
	definition.name = "GodotHTTP";
	definition.factory = [p_service, p_blobs] { return std::make_unique<RNHTTPModule>(p_service, p_blobs); };
	RNMethodSchema send;
	send.name = "send";
	send.mode = RNCallMode::ASYNC;
	send.result = RNValueSchema::value(RNValueType::DYNAMIC);
	RNArgumentSchema request;
	request.name = "request";
	request.value = RNValueSchema::value(RNValueType::DYNAMIC);
	send.arguments.push_back(request);
	definition.methods.push_back(send);
	RNMethodSchema stats;
	stats.name = "stats";
	stats.result = RNValueSchema::value(RNValueType::DYNAMIC);
	definition.methods.push_back(stats);
	RNMethodSchema clear;
	clear.name = "clearCookies";
	clear.result = RNValueSchema::value(RNValueType::BOOL);
	definition.methods.push_back(clear);
	RNMethodSchema trust;
	trust.name = "setTrustResource";
	trust.result = RNValueSchema::value(RNValueType::VOID);
	RNArgumentSchema path;
	path.name = "path";
	path.value = RNValueSchema::value(RNValueType::STRING);
	trust.arguments.push_back(path);
	definition.methods.push_back(trust);
	return p_registry.register_module(definition, r_error);
}
