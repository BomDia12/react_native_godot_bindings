#pragma once

#include "../runtime/rn_service_settings.h"
#include "rn_cookie_jar.h"
#include "rn_image_service.h"

#include "scene/main/http_request.h"

#include <deque>
#include <map>

struct RNHTTPRequest {
	String url;
	String method = "GET";
	PackedStringArray headers;
	PackedByteArray body;
	double timeout_ms = 0;
	uint64_t response_limit = 0;
	bool credentials = true;
};
struct RNHTTPResponse {
	String url;
	int status = 0;
	PackedStringArray headers;
	PackedByteArray body;
	RNError error;
};
class RNHTTPService : public Node {
	GDCLASS(RNHTTPService, Node);
	struct Request {
		uint64_t id = 0;
		RNHTTPRequest spec;
		double deadline = 0;
		int redirects = 0;
		bool cancelled = false;
		std::function<void(RNHTTPResponse)> completion;
	};
	enum class State { IDLE,
		LEASED,
		DRAINING };
	struct Slot {
		ObjectID node;
		State state = State::IDLE;
		uint64_t sequence = 0;
		uint64_t request = 0;
		uint64_t reservation = 0;
		Callable callback;
	};
	RNServiceSettings settings;
	RNCookieJar cookies;
	std::map<uint64_t, Request> requests;
	std::deque<uint64_t> waiting;
	std::vector<Slot> slots;
	uint64_t next_id = 1;
	uint64_t buffered = 0;
	uint64_t started = 0;
	bool closed = false;
	Ref<TLSOptions> tls_options;
	static double now();
	void lease(size_t p_slot, uint64_t p_request);
	void settle(uint64_t p_request, RNHTTPResponse p_response);
	void quarantine(size_t p_slot);
	void _completed(int p_result, int p_status, const PackedStringArray &p_headers, const PackedByteArray &p_body, int p_slot, int64_t p_sequence);
	void _drained(int p_slot, int64_t p_sequence);

protected:
	static void _bind_methods();

public:
	explicit RNHTTPService(const RNServiceSettings &p_settings);
	~RNHTTPService() override;
	uint64_t submit(const RNHTTPRequest &p_request, std::function<void(RNHTTPResponse)> p_completion, RNError &r_error);
	bool cancel(uint64_t p_request);
	void process_requests();
	bool has_pending_work() const;
	bool clear_cookies() { return cookies.clear(); }
	void shutdown();
	Dictionary stats() const;
	uint64_t upload_limit() const { return settings.limit("network/http/max_buffered_bytes"); }
	bool set_trust_resource(const String &p_path, RNError &r_error);
};
std::shared_ptr<RNImageTransport> rn_http_image_transport(const std::shared_ptr<RNHTTPService> &p_service);
class RNBlobService;
bool rn_register_http_module(RNNativeModuleRegistry &p_registry, const std::function<std::shared_ptr<RNHTTPService>()> &p_service, const std::function<std::shared_ptr<RNBlobService>()> &p_blobs, RNError &r_error);
