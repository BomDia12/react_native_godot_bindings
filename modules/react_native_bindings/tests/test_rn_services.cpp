#include "../host/rn_service_settings.h"
#include "../interop/rn_scene_binding.h"
#include "../native_modules/rn_blob_service.h"
#include "../native_modules/rn_cookie_jar.h"
#include "../native_modules/rn_http_service.h"
#include "../root_view/react_native_root_view.h"
#include "../turbomodules/rn_godot_alerts.h"
#include "../turbomodules/rn_godot_scene_module.h"
#include "../turbomodules/rn_network_modules.h"
#include "support/rn_test_runtime.h"
#include "support/rn_upstream_test_macros.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "scene/2d/node_2d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

#include "modules/websocket/websocket_peer.h"

#include <algorithm>

namespace TestRNServices {

using namespace facebook;
using facebook::react::RNWebSocketHub;

class SocketFixturePeer : public WebSocketPeer {
	State state = STATE_CONNECTING;
	bool packet_available = true;

public:
	static inline int failure_mode = 0;
	using Factory = WebSocketPeer *(*)(bool);
	static Factory exchange_factory(Factory p_factory) {
		const Factory previous = _create;
		_create = p_factory;
		return previous;
	}
	static WebSocketPeer *create_fixture(bool p_notify) {
		return static_cast<WebSocketPeer *>(ClassDB::creator<SocketFixturePeer>(p_notify));
	}
	Error connect_to_url(const String &, const Ref<TLSOptions> &) override {
		if (failure_mode == 1 || failure_mode == 2) {
			state = STATE_CLOSED;
		}
		return failure_mode == 2 ? ERR_CANT_CONNECT : OK;
	}
	Error accept_stream(const Ref<StreamPeer> &) override { return ERR_UNAVAILABLE; }
	Error send(const uint8_t *, int, WriteMode) override { return OK; }
	void close(int = 1000, const String & = "") override { state = STATE_CLOSED; }
	IPAddress get_connected_host() const override { return IPAddress(); }
	uint16_t get_connected_port() const override { return 0; }
	bool was_string_packet() const override { return true; }
	void set_no_delay(bool) override {}
	int get_current_outbound_buffered_amount() const override { return 0; }
	String get_selected_protocol() const override { return "fixture"; }
	String get_requested_url() const override { return "ws://localhost/test"; }
	void poll() override {
		if (state == STATE_CONNECTING) {
			state = STATE_OPEN;
		}
	}
	State get_ready_state() const override { return state; }
	int get_close_code() const override { return 1000; }
	String get_close_reason() const override { return String(); }
	Error get_packet(const uint8_t **r_buffer, int &r_size) override {
		if (failure_mode == 3) {
			return ERR_CANT_CONNECT;
		}
		static const uint8_t packet[] = { 'h', 'i' };
		*r_buffer = packet;
		r_size = 2;
		packet_available = false;
		return OK;
	}
	Error put_packet(const uint8_t *, int) override { return OK; }
	int get_available_packet_count() const override { return state == STATE_OPEN && packet_available ? 1 : 0; }
	int get_max_packet_size() const override { return 2; }
};
struct SocketFactoryGuard {
	SocketFixturePeer::Factory previous = SocketFixturePeer::exchange_factory(SocketFixturePeer::create_fixture);
	~SocketFactoryGuard() {
		SocketFixturePeer::exchange_factory(previous);
		SocketFixturePeer::failure_mode = 0;
	}
};
Dictionary schema(const String &p_name) {
	Dictionary result;
	result["type"] = p_name;
	return result;
}
TEST_CASE("[ReactNativeBindings][SceneBinding] native signatures remain generic and reject incompatible command types") {
	Node2D *target = memnew(Node2D);
	Ref<RNSceneBinding> resource;
	resource.instantiate();
	resource->set_capability("TransformProbe");
	resource->set_snapshot_method("get_position");
	resource->set_snapshot_schema(schema("Vector2"));
	Dictionary argument;
	argument["name"] = "position";
	argument["value"] = schema("Vector2");
	Array arguments;
	arguments.push_back(argument);
	Dictionary command;
	command["method"] = "set_position";
	command["mode"] = "sync";
	command["arguments"] = arguments;
	command["result"] = schema("void");
	Dictionary commands;
	commands["move"] = command;
	resource->set_commands(commands);
	RNSceneAttachment attachment;
	RNError error;
	CHECK(resource->compile(target, attachment, error));
	argument["value"] = schema("string");
	CHECK_FALSE(resource->compile(target, attachment, error));
	memdelete(target);
}
TEST_CASE("[ReactNativeBindings][Blob] slices pins and bounded reads preserve zero bytes") {
	auto service = std::make_shared<RNBlobService>(32 * 1024 * 1024);
	PackedByteArray bytes;
	bytes.resize(17 * 1024 * 1024 + 3);
	bytes.set(0, 65);
	bytes.set(1, 0);
	bytes.set(bytes.size() - 1, 255);
	RNError error;
	Dictionary descriptor = service->store(bytes, error);
	REQUIRE_FALSE(error.is_set());
	CHECK(service->used_bytes() == uint64_t(bytes.size()));
	PackedByteArray chunk;
	CHECK_FALSE(service->read(descriptor, 0, 1024 * 1024 + 1, chunk, error));
	REQUIRE(service->read(descriptor, 0, 3, chunk, error));
	CHECK(chunk[0] == 65);
	CHECK(chunk[1] == 0);
	REQUIRE(service->pin(descriptor, error));
	Dictionary slice = descriptor.duplicate();
	slice["offset"] = bytes.size() - 2;
	slice["size"] = 2;
	const String id = descriptor["blobId"];
	service->release(id);
	service->release(id);
	REQUIRE(service->read(slice, 0, 2, chunk, error));
	CHECK(chunk[1] == 255);
	CHECK(service->count() == 1);
	service->unpin(id);
	CHECK(service->count() == 0);
	CHECK(service->used_bytes() == 0);
	CHECK_FALSE(service->read(descriptor, 0, 1, chunk, error));
}
TEST_CASE("[ReactNativeBindings][Blob] creation collector reset and zero-size allocation limits") {
	auto service = std::make_shared<RNBlobService>(1024);
	RNError error;
	PackedByteArray bytes;
	bytes.resize(2048);
	CHECK_FALSE(service->create("oversized", bytes, error));
	bytes.resize(4);
	REQUIRE(service->create("small", bytes, error));
	CHECK_FALSE(service->create("small", bytes, error));
	service->release("small");
	CHECK(service->used_bytes() == 0);
	REQUIRE(service->create("collected", PackedByteArray(), error));
	RNTestRuntime runtime;
	runtime.global().setProperty(runtime, "__collectors", jsi::Object::createFromHostObject(runtime, service->collector_provider()));
	runtime.evaluate("globalThis.collector=__collectors.create('collected');");
	runtime.evaluate("collector=null;");
	runtime.collect_garbage();
	service->drain_releases();
	CHECK(service->count() == 0);
	for (int i = 0; i < 4096; ++i) {
		REQUIRE(service->create(String::num_int64(i), PackedByteArray(), error));
	}
	CHECK_FALSE(service->create("overflow", PackedByteArray(), error));
	service->shutdown();
	CHECK(service->count() == 0);
	CHECK_FALSE(service->create("stale", PackedByteArray(), error));
}
TEST_CASE("[ReactNativeBindings][Cookies] host path secure expiry repeated headers and replacement bounds") {
	RNCookieJar jar(4, 1024);
	PackedStringArray headers;
	headers.push_back("Set-Cookie: a=one; Path=/");
	headers.push_back("Set-Cookie: b=two; Path=/inner; Secure");
	headers.push_back("Set-Cookie: expires=soon; Max-Age=2");
	jar.receive("https://example.test/inner/start", headers, 100);
	CHECK(jar.header("https://example.test/inner/a", 101).contains("a=one"));
	CHECK(jar.header("https://example.test/inner/a", 101).begins_with("b=two"));
	CHECK_FALSE(jar.header("http://example.test/inner/a", 101).contains("b=two"));
	CHECK_FALSE(jar.header("https://other.test/inner/a", 101).contains("a=one"));
	CHECK_FALSE(jar.header("https://example.test/inner/a", 103).contains("expires="));
	headers.clear();
	headers.push_back("Set-Cookie: a=replaced; Path=/");
	jar.receive("https://example.test/", headers, 104);
	CHECK(jar.header("https://example.test/", 104) == "a=replaced");
	CHECK(jar.clear());
	CHECK_FALSE(jar.clear());
}
TEST_CASE("[ReactNativeBindings][Settings] immutable snapshots reject bad limits and allow documented zero values") {
	auto project = ProjectSettings::get_singleton();
	const String key = "react_native/network/http/max_queued_requests";
	const Variant previous = project->get_setting(key, 128);
	RNServiceSettings settings;
	RNError error;
	project->set_setting(key, 0);
	REQUIRE(RNServiceSettings::snapshot(settings, error));
	CHECK(settings.limit("network/http/max_queued_requests") == 0);
	project->set_setting(key, 5);
	CHECK(settings.limit("network/http/max_queued_requests") == 0);
	project->set_setting(key, -1);
	CHECK_FALSE(RNServiceSettings::snapshot(settings, error));
	project->set_setting(key, previous);
	const String scale = "react_native/text/font_scale";
	const Variant old_scale = project->get_setting(scale, 1.0);
	project->set_setting(scale, 0.0);
	CHECK_FALSE(RNServiceSettings::snapshot(settings, error));
	project->set_setting(scale, old_scale);
}
TEST_CASE("[ReactNativeBindings][HTTP] redirects resolve query fragment relative paths and dot segments") {
	const String base = "https://example.test/foo/bar?old=1";
	const std::pair<const char *, const char *> cases[] = {
		{ "?page=2", "/foo/bar?page=2" },
		{ "#section", "/foo/bar?old=1#section" },
		{ "", "/foo/bar?old=1" },
		{ "next", "/foo/next" },
		{ "../next", "/next" },
		{ "./next?query=/../value", "/foo/next?query=/../value" },
		{ "/a/../b//c", "/b//c" },
		{ "/.", "/" },
		{ "/..", "/" },
		{ "%2e%2e/next", "/foo/%2e%2e/next" },
	};
	String resolved;
	for (const auto &entry : cases) {
		REQUIRE(RNParsedURL::resolve(base, entry.first, resolved));
		CHECK(resolved == "https://example.test:443" + String(entry.second));
	}
	REQUIRE(RNParsedURL::resolve("https://example.test/", "next", resolved));
	CHECK(resolved == "https://example.test:443/next");
	REQUIRE(RNParsedURL::resolve(base, "//other.test/a", resolved));
	CHECK(resolved == "https://other.test:443/a");
	REQUIRE(RNParsedURL::resolve(base, "http://other.test?x=1", resolved));
	CHECK(resolved == "http://other.test:80/?x=1");
	REQUIRE(RNParsedURL::resolve("http://[::1]/a", "b", resolved));
	CHECK(resolved == "http://[::1]:80/b");
	CHECK_FALSE(RNParsedURL::resolve(base, "mailto:other.test", resolved));
}
TEST_CASE("[ReactNativeBindings][Settings] HTTP response reservations fit the aggregate buffer ceiling") {
	auto project = ProjectSettings::get_singleton();
	const String key = "react_native/network/http/max_buffered_bytes";
	const Variant previous = project->get_setting(key, 32 * 1024 * 1024);
	const int64_t body_limit = project->get_setting("react_native/network/http/max_body_bytes", 8 * 1024 * 1024);
	RNServiceSettings settings;
	RNError error;
	project->set_setting(key, body_limit - 1);
	CHECK_FALSE(RNServiceSettings::snapshot(settings, error));
	CHECK(error.code == RNErrorCode::VALIDATION);
	CHECK(error.path == "react_native/network/http/max_body_bytes");
	project->set_setting(key, body_limit);
	CHECK(RNServiceSettings::snapshot(settings, error));
	project->set_setting(key, previous);
}
TEST_CASE("[ReactNativeBindings][WebSocket] sockets report open before their first message") {
	SocketFactoryGuard guard;
	RNServiceSettings settings;
	RNError error;
	REQUIRE(RNServiceSettings::snapshot(settings, error));
	RNWebSocketHub hub(nullptr, settings);
	std::vector<std::string> events;
	hub.set_event_emitter([&events](const std::string &p_name, folly::dynamic) { events.push_back(p_name); });
	hub.connect(123, "ws://localhost/test", PackedStringArray(), PackedStringArray());
	hub.process_frame();
	hub.process_frame();
	CHECK(events == std::vector<std::string>({ "websocketOpen", "websocketMessage" }));
	hub.shutdown();
}
TEST_CASE("[ReactNativeBindings][Cookies] Domain attributes cannot cross registrants or shared hosting tenants") {
	RNCookieJar jar(8, 4096);
	for (const String &host : { String("attacker.co.uk"), String("tenant.github.io"), String("sub.example.test") }) {
		PackedStringArray headers;
		headers.push_back("Set-Cookie: attack=one; Domain=" + host.substr(host.find(".") + 1) + "; Path=/");
		headers.push_back("Set-Cookie: exact=two; Domain=" + host + "; Path=/");
		headers.push_back("Set-Cookie: local=three; Path=/");
		jar.receive("https://" + host + "/", headers, 100);
		CHECK(jar.header("https://" + host + "/", 100) == "local=three");
		CHECK(jar.header("https://victim.co.uk/", 100).is_empty());
		CHECK(jar.header("https://other.github.io/", 100).is_empty());
		CHECK(jar.header("https://child." + host + "/", 100).is_empty());
		jar.clear();
	}
}
TEST_CASE("[ReactNativeBindings][WebSocket] failed handshakes and inbound errors report one failure and release the peer") {
	SocketFactoryGuard guard;
	SUBCASE("closed handshake") {
		SocketFixturePeer::failure_mode = 1;
	}
	SUBCASE("immediate connect failure") {
		SocketFixturePeer::failure_mode = 2;
	}
	SUBCASE("inbound packet failure") {
		SocketFixturePeer::failure_mode = 3;
	}
	RNServiceSettings settings;
	RNError error;
	REQUIRE(RNServiceSettings::snapshot(settings, error));
	RNWebSocketHub hub(nullptr, settings);
	std::vector<std::string> events;
	hub.set_event_emitter([&events](const std::string &p_name, folly::dynamic) { events.push_back(p_name); });
	hub.connect(123, "ws://localhost/test", PackedStringArray(), PackedStringArray());
	for (int i = 0; i < 3; ++i) {
		hub.process_frame();
	}
	CHECK(std::count(events.begin(), events.end(), "websocketFailed") == 1);
	CHECK(std::count(events.begin(), events.end(), "websocketClosed") == 0);
	CHECK(hub.get_socket_count() == 0);
}
struct SceneFixture {
	ReactNativeRootView *root = memnew(ReactNativeRootView);
	std::shared_ptr<RNSceneBindings> scenes = std::make_shared<RNSceneBindings>([this](int p_root_tag) { return p_root_tag == 11 ? root : nullptr; });
	std::vector<Variant> changes;
	SceneFixture() {
		scenes->add_emitter([this](const Variant &p_change) {
			changes.push_back(p_change);
			return true;
		});
	}
	~SceneFixture() {
		scenes->shutdown();
		memdelete(root);
	}
};
TEST_CASE("[ReactNativeBindings][SceneBinding] signals publish ordered changes and replacements issue new handles") {
	SceneFixture fixture;
	Ref<Resource> target;
	target.instantiate();
	Ref<RNSceneBinding> resource;
	resource.instantiate();
	resource->set_capability("ResourceProbe");
	resource->set_snapshot_method("get_path");
	resource->set_snapshot_schema(schema("string"));
	Dictionary payload_schema = schema("record");
	payload_schema["fields"] = Dictionary();
	Dictionary signal;
	signal["event"] = "updated";
	signal["arguments"] = Array();
	signal["payload"] = payload_schema;
	Dictionary signals;
	signals["changed"] = signal;
	resource->set_signals(signals);
	REQUIRE(fixture.root->attach_scene_binding(target.ptr(), resource).is_empty());
	Dictionary before;
	RNError error;
	REQUIRE(fixture.scenes->get_binding(11, before, error));
	CHECK(bool(before["ready"]));
	fixture.changes.clear();
	target->emit_changed();
	REQUIRE(fixture.changes.size() == 1);
	const Dictionary updated = fixture.changes[0];
	CHECK(updated["event"] == Variant("updated"));
	CHECK(updated["binding"] == before["binding"]);
	CHECK(int64_t(updated["sequence"]) > int64_t(before["sequence"]));
	REQUIRE(fixture.root->attach_scene_binding(target.ptr(), resource).is_empty());
	fixture.scenes->binding_changed(11);
	REQUIRE(fixture.changes.size() == 2);
	const Dictionary replaced = fixture.changes[1];
	CHECK(bool(replaced["ready"]));
	CHECK(replaced["binding"] != before["binding"]);
	Dictionary unknown;
	CHECK_FALSE(fixture.scenes->get_binding(21, unknown, error));
	CHECK(error.code == RNErrorCode::STALE_HANDLE);
}
void record_alert(const String &p_request, const Variant &, const Dictionary &, Array p_requests) {
	p_requests.push_back(p_request);
}
TEST_CASE("[ReactNativeBindings][Alerts] custom presentations without a presenter bubble and stale replies are rejected") {
	ReactNativeRootView *root = memnew(ReactNativeRootView);
	Array requests;
	root->set_alert_handler(callable_mp_static(&record_alert).bind(requests));
	Dictionary button;
	button["text"] = "OK";
	button["style"] = "default";
	Dictionary payload;
	payload["title"] = "Test";
	payload["message"] = "Message";
	payload["cancelable"] = false;
	payload["buttons"] = Array({ button });
	Dictionary settled;
	RNError failure;
	GodotAlerts::get_singleton()->request(root, 11, payload, "custom", [&](const Dictionary &p_result, const RNError &p_error) {
		settled = p_result;
		failure = p_error;
	});
	MessageQueue::get_singleton()->flush();
	REQUIRE(requests.size() == 1);
	Dictionary result;
	result["buttonId"] = 0;
	result["dismissed"] = false;
	RNError error;
	CHECK_FALSE(GodotAlerts::get_singleton()->reply(11, requests[0], result, error));
	CHECK(error.code == RNErrorCode::CANCELLED);
	CHECK(root->complete_alert(requests[0], result));
	CHECK(settled == result);
	CHECK_FALSE(failure.is_set());
	GodotAlerts::get_singleton()->request(nullptr, 21, payload, "native", [&](const Dictionary &, const RNError &p_error) { failure = p_error; });
	CHECK(failure.code == RNErrorCode::CANCELLED);
	memdelete(root);
}
TEST_CASE("[ReactNativeBindings][Blob] collecting one wrapper preserves live sibling collectors and native pins") {
	auto service = std::make_shared<RNBlobService>(1024);
	PackedByteArray bytes;
	bytes.push_back(65);
	bytes.push_back(0);
	bytes.push_back(66);
	RNError error;
	Dictionary data = service->store(bytes, error);
	REQUIRE_FALSE(error.is_set());
	const String id = data["blobId"];
	RNTestRuntime runtime;
	runtime.global().setProperty(runtime, "__collectors", jsi::Object::createFromHostObject(runtime, service->collector_provider()));
	runtime.global().setProperty(runtime, "blobId", jsi::String::createFromUtf8(runtime, id.utf8().get_data()));
	runtime.evaluate("globalThis.first=__collectors.create(blobId);globalThis.second=__collectors.create(blobId);");
	runtime.evaluate("first=null;");
	runtime.collect_garbage();
	service->drain_releases();
	CHECK(service->count() == 1);
	PackedByteArray chunk;
	CHECK(service->read(data, 0, 3, chunk, error));
	CHECK(chunk == bytes);
	runtime.evaluate("second=null;");
	runtime.collect_garbage();
	CHECK(service->has_pending_work());
	runtime.evaluate("globalThis.replacement=__collectors.create(blobId);");
	service->drain_releases();
	CHECK(service->count() == 1);
	CHECK(service->read(data, 0, 3, chunk, error));
	REQUIRE(service->pin(data, error));
	runtime.evaluate("replacement=null;");
	runtime.collect_garbage();
	service->drain_releases();
	CHECK(service->count() == 1);
	CHECK(service->read(data, 0, 3, chunk, error));
	service->unpin(id);
	CHECK(service->count() == 0);
	CHECK(service->used_bytes() == 0);
}
TEST_CASE("[ReactNativeBindings][Cookies] oversized replacements preserve valid entries and explicit expiry still deletes") {
	RNCookieJar jar(4, 128);
	jar.receive("https://example.test/", { "Set-Cookie: session=good; Path=/" }, 100);
	for (const String &suffix : { String(), String("; Max-Age=100"), String("; Expires=Wed, 09 Jun 2032 10:18:14 GMT") }) {
		jar.receive("https://example.test/", { "Set-Cookie: session=" + String("x").repeat(256) + "; Path=/" + suffix }, 101);
		CHECK(jar.header("https://example.test/", 101) == "session=good");
	}
	jar.receive("https://example.test/", { "Set-Cookie: other=kept; Path=/" }, 102);
	CHECK(jar.header("https://example.test/", 102) == "other=kept");
	jar.receive("https://example.test/", { "Set-Cookie: other=" + String("x").repeat(256) + "; Path=/; Max-Age=0" }, 103);
	CHECK(jar.header("https://example.test/", 103).is_empty());
	jar.receive("https://example.test/", { "Set-Cookie: short=old; Path=/", "Set-Cookie: short=new; Path=/" }, 104);
	CHECK(jar.header("https://example.test/", 104) == "short=new");
}

TEST_CASE("[ReactNativeBindings][Cookies] insecure cookies cannot replace delete or overlay live Secure cookies") {
	RNCookieJar jar(32, 4096);
	jar.receive("https://example.test/login", { "Set-Cookie: session=good; Secure; Path=/login" }, 100);
	for (const String &header : { String("Set-Cookie: session=evil; Path=/login"), String("Set-Cookie: session=deleted; Path=/login; Max-Age=0"), String("Set-Cookie: session=child; Path=/login/en"), String("Set-Cookie: session=child; Path=/login/en; Max-Age=0") }) {
		jar.receive("http://example.test/login", { header }, 101);
		CHECK(jar.header("https://example.test/login/en", 101) == "session=good");
	}
	jar.receive("http://child.example.test/login", { "Set-Cookie: session=child; Path=/login" }, 101);
	CHECK(jar.header("https://child.example.test/login", 101).is_empty());
	jar.receive("http://other.test/login", { "Set-Cookie: session=other; Path=/login" }, 101);
	CHECK(jar.header("http://other.test/login", 101) == "session=other");
	jar.receive("http://example.test/", { "Set-Cookie: session=wide; Path=/", "Set-Cookie: session=unrelated; Path=/login-other" }, 101);
	CHECK(jar.header("https://example.test/login/en", 101) == "session=good; session=wide");
	CHECK(jar.header("http://example.test/login-other", 101) == "session=unrelated; session=wide");
	jar.receive("https://example.test/login", { "Set-Cookie: session=replaced; Path=/login" }, 102);
	CHECK(jar.header("https://example.test/login", 102) == "session=replaced; session=wide");
	jar.receive("https://example.test/", { "Set-Cookie: expired=old; Secure; Path=/; Max-Age=1" }, 100);
	jar.receive("http://example.test/", { "Set-Cookie: expired=new; Path=/" }, 102);
	CHECK(jar.header("http://example.test/", 102) == "session=wide; expired=new");
	jar.receive("https://example.test/", { "Set-Cookie: root=good; Secure; Path=/" }, 102);
	jar.receive("http://example.test/", { "Set-Cookie: root=deleted; Path=/; Max-Age=0", "Set-Cookie: root=child; Path=/child" }, 103);
	CHECK(jar.header("https://example.test/child", 103) == "session=wide; expired=new; root=good");
}

TEST_CASE("[ReactNativeBindings][Cookies] secure prefixes reject insecure replacements and require an explicit root path") {
	RNCookieJar jar(32, 4096);
	jar.receive("https://example.test/", { "Set-Cookie: __Host-session=good; Secure; Path=/", "Set-Cookie: __Secure-token=good; Secure; Path=/" }, 100);
	const String expected = "__Host-session=good; __Secure-token=good";
	REQUIRE(jar.header("https://example.test/", 100) == expected);
	for (const String &header : { String("Set-Cookie: __Host-session=evil; Path=/"), String("Set-Cookie: __Host-session=gone; Path=/; Max-Age=0"), String("Set-Cookie: __Secure-token=evil; Path=/"), String("Set-Cookie: __Secure-token=evil; Secure; Path=/") }) {
		jar.receive("http://example.test/", { header }, 101);
		CHECK(jar.header("https://example.test/", 101) == expected);
	}
	for (const String &header : { String("Set-Cookie: __Host-session=evil; Path=/"), String("Set-Cookie: __Host-session=evil; Secure"), String("Set-Cookie: __Host-session=evil; Secure; Path=/inner"), String("Set-Cookie: __Host-session=evil; Secure; Domain=example.test; Path=/"), String("Set-Cookie: __Host-session=evil; Secure; Path=invalid"), String("Set-Cookie: __Secure-token=evil; Path=/"), String("Set-Cookie: __hOsT-mixed=evil; Secure"), String("Set-Cookie: __sEcUrE-mixed=evil") }) {
		jar.receive("https://example.test/", { header }, 102);
		CHECK(jar.header("https://example.test/", 102) == expected);
	}
	CHECK(jar.header("http://example.test/", 102).is_empty());
	jar.receive("https://example.test/", { "Set-Cookie: __Host-session=new; Secure; Path=/", "Set-Cookie: __hOsT-mixed=valid; Secure; Path=/", "Set-Cookie: __sEcUrE-mixed=valid; Secure; Path=/" }, 103);
	CHECK(jar.header("https://example.test/", 103).contains("__Host-session=new"));
	CHECK(jar.header("https://example.test/", 103).contains("__hOsT-mixed=valid"));
	CHECK(jar.header("https://example.test/", 103).contains("__sEcUrE-mixed=valid"));
	jar.receive("https://example.test/", { "Set-Cookie: __Host-session=deleted; Secure; Path=/; Max-Age=0" }, 104);
	CHECK_FALSE(jar.header("https://example.test/", 104).contains("__Host-session="));
}
TEST_CASE("[ReactNativeBindings][SceneBinding][SceneTree] Object commands resolve root-scoped capabilities and wrap native results") {
	SceneFixture fixture;
	Node2D *target = memnew(Node2D);
	Node *child = memnew(Node);
	target->add_child(child);
	SceneTree::get_singleton()->get_root()->add_child(target);
	Ref<RNSceneBinding> resource;
	resource.instantiate();
	resource->set_capability("TreeProbe");
	resource->set_snapshot_method("get_parent");
	Dictionary object_schema = schema("Object");
	object_schema["capability"] = "TreeNode";
	resource->set_snapshot_schema(object_schema);
	Dictionary object_argument;
	object_argument["name"] = "node";
	object_argument["value"] = object_schema;
	Dictionary ancestor;
	ancestor["method"] = "is_ancestor_of";
	ancestor["mode"] = "sync";
	ancestor["arguments"] = Array({ object_argument });
	ancestor["result"] = schema("boolean");
	Dictionary index_argument;
	index_argument["name"] = "index";
	index_argument["value"] = schema("integer");
	Dictionary internal_argument;
	internal_argument["name"] = "include_internal";
	internal_argument["value"] = schema("boolean");
	internal_argument["optional"] = true;
	Dictionary get_child;
	get_child["method"] = "get_child";
	get_child["mode"] = "sync";
	get_child["arguments"] = Array({ index_argument, internal_argument });
	get_child["result"] = object_schema;
	Dictionary commands;
	commands["ancestor"] = ancestor;
	commands["child"] = get_child;
	get_child = get_child.duplicate();
	get_child["mode"] = "queued";
	commands["childAsync"] = get_child;
	resource->set_commands(commands);
	REQUIRE(fixture.root->attach_scene_binding(target, resource).is_empty());
	RNError error;
	Dictionary binding;
	REQUIRE(fixture.scenes->get_binding(11, binding, error));
	const String handle = binding["binding"];
	Variant child_handle;
	REQUIRE(fixture.scenes->invoke(11, handle, "child", Array({ 0 }), false, child_handle, error));
	Variant snapshot;
	REQUIRE(fixture.scenes->invoke(11, handle, StringName(), Array(), false, snapshot, error));
	const Dictionary parent_handle = Dictionary(snapshot)["payload"];
	CHECK(Dictionary(child_handle)["$godot"] == Variant("Object"));
	CHECK(parent_handle["$godot"] == Variant("Object"));
	const Variant child_token = Dictionary(child_handle)["handle"];
	Variant is_ancestor;
	REQUIRE(fixture.scenes->invoke(11, handle, "ancestor", Array({ child_token }), false, is_ancestor, error));
	CHECK(bool(is_ancestor));
	REQUIRE(fixture.scenes->invoke(11, handle, "ancestor", Array({ parent_handle["handle"] }), false, is_ancestor, error));
	CHECK_FALSE(bool(is_ancestor));
	CHECK_FALSE(fixture.scenes->invoke(11, handle, "child", Array({ 0 }), true, child_handle, error));
	CHECK(error.code == RNErrorCode::VALIDATION);
	error = RNError();
	Variant async_child;
	REQUIRE(fixture.scenes->invoke(11, handle, "childAsync", Array({ 0 }), true, async_child, error));
	CHECK(Dictionary(async_child)["handle"] == Dictionary(child_handle)["handle"]);
	memdelete(child);
	CHECK_FALSE(fixture.scenes->invoke(11, handle, "ancestor", Array({ child_token }), false, is_ancestor, error));
	CHECK(error.code == RNErrorCode::OBJECT_GONE);
	memdelete(target);
}
TEST_CASE("[ReactNativeBindings][HTTP][SceneTree] immediate native start failures settle and leave no pending work") {
	RNServiceSettings settings;
	RNError error;
	REQUIRE(RNServiceSettings::snapshot(settings, error));
	RNHTTPService *service = memnew(RNHTTPService(settings));
	SceneTree::get_singleton()->get_root()->add_child(service);
	int failures = 0;
	for (int i = 0; i < 20; ++i) {
		RNHTTPRequest request;
		request.url = "http://localhost?unused:invalid";
		request.method = "GET";
		ERR_PRINT_OFF;
		service->submit(request, [&failures](RNHTTPResponse p_response) { failures += p_response.error.is_set() ? 1 : 0; }, error);
		service->process_requests();
		ERR_PRINT_ON;
		MessageQueue::get_singleton()->flush();
		CHECK_FALSE(service->has_pending_work());
	}
	CHECK(failures == 20);
	service->shutdown();
	memdelete(service);
}
} //namespace TestRNServices
void rn_force_link_service_tests() {}
