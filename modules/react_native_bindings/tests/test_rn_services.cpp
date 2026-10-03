#include "../components/rn_builtin_descriptors.h"
#include "../components/rn_font.h"
#include "../components/rn_text_control.h"
#include "../fabric/rn_shadow_node.h"
#include "../interop/rn_scene_binding.h"
#include "../native_modules/rn_alert_service.h"
#include "../native_modules/rn_application_services.h"
#include "../native_modules/rn_blob_service.h"
#include "../native_modules/rn_cookie_jar.h"
#include "../native_modules/rn_godot_scene_module.h"
#include "../native_modules/rn_websocket_module.h"
#include "../root_view/react_native_root_view.h"
#include "../runtime/react_native_runtime_coordinator.h"
#include "../runtime/rn_service_settings.h"
#include "../singletons/hermes_runtime_singleton.h"

#include "core/config/project_settings.h"
#include "scene/2d/node_2d.h"
#include "tests/test_macros.h"

#include "modules/websocket/websocket_peer.h"

namespace TestRNServices {
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
TEST_CASE("[ReactNativeBindings][FontScale] scale is rounded once and caps preserve dependency invalidation") {
	RNHostContext context;
	context.font_scale = 2.25;
	Dictionary props;
	props["fontSize"] = 13.5;
	RNFontSnapshot font;
	RNError error;
	REQUIRE(rn_resolve_font(props, context, font, error));
	CHECK(font.size == 30);
	CHECK(font.multiplier == 2.25);
	const uint64_t first_revision = font.revision;
	++context.metrics_revision;
	REQUIRE(rn_resolve_font(props, context, font, error));
	CHECK(font.revision != first_revision);
	props["maxFontSizeMultiplier"] = 1.5;
	REQUIRE(rn_resolve_font(props, context, font, error));
	CHECK(font.size == 20);
	props["maxFontSizeMultiplier"] = 0;
	REQUIRE(rn_resolve_font(props, context, font, error));
	CHECK(font.size == 30);
	props["allowFontScaling"] = false;
	REQUIRE(rn_resolve_font(props, context, font, error));
	CHECK(font.size == 14);
	props["maxFontSizeMultiplier"] = 0.5;
	CHECK_FALSE(rn_resolve_font(props, context, font, error));
}
TEST_CASE("[ReactNativeBindings][Blob] slices pins object URLs and bounded reads preserve zero bytes") {
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
	String url = service->create_url(slice, error);
	REQUIRE_FALSE(url.is_empty());
	const String id = descriptor["blobId"];
	service->release(id);
	service->release(id);
	REQUIRE(service->resolve_url(url, slice, error));
	REQUIRE(service->read(slice, 0, 2, chunk, error));
	CHECK(chunk[1] == 255);
	service->unpin(id);
	CHECK(service->count() == 1);
	service->revoke_url(url);
	CHECK(service->count() == 0);
	CHECK(service->used_bytes() == 0);
	CHECK_FALSE(service->resolve_url(url, slice, error));
	CHECK_FALSE(service->read(descriptor, 0, 1, chunk, error));
}
TEST_CASE("[ReactNativeBindings][Blob] partial writes collector reset and zero-size allocation limits") {
	auto service = std::make_shared<RNBlobService>(1024);
	RNError error;
	REQUIRE(service->begin("partial", 4, error));
	CHECK_FALSE(service->finish("partial", error));
	PackedByteArray bytes;
	bytes.push_back(0);
	CHECK_FALSE(service->append("partial", 1, bytes, error));
	CHECK(service->append("partial", 0, bytes, error));
	service->release("partial");
	CHECK(service->used_bytes() == 0);
	REQUIRE(service->begin("collected", 0, error));
	REQUIRE(service->finish("collected", error));
	auto runtime = HermesRuntimeSingleton::get_singleton();
	runtime->reset();
	runtime->install_host_object("__collectors", service->collector_provider());
	runtime->evaluate("globalThis.collector=__collectors.create('collected');undefined;");
	REQUIRE(runtime->get_last_error().is_empty());
	runtime->evaluate("collector=null;undefined;");
	runtime->collect_garbage();
	service->drain_releases();
	CHECK(service->count() == 0);
	for (int i = 0; i < 4096; ++i) {
		REQUIRE(service->begin(String::num_int64(i), 0, error));
	}
	CHECK_FALSE(service->begin("overflow", 0, error));
	service->shutdown();
	CHECK(service->count() == 0);
	CHECK_FALSE(service->begin("stale", 0, error));
	runtime->uninstall_host_object("__collectors");
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
TEST_CASE("[ReactNativeBindings][FontScale][SceneTree] inherited capped disabled and unlimited spans share scratch and live shaping") {
	RNHostDescriptorRegistry registry;
	RNError error;
	REQUIRE(rn_register_builtin_descriptors(registry, error));
	Ref<RNShadowNode> text;
	text.instantiate();
	text->view_name = "RCTText";
	text->descriptor = registry.find("RCTText");
	text->props["fontSize"] = 20;
	text->props["lineHeight"] = 24;
	text->props["maxFontSizeMultiplier"] = 1.5;
	for (int i = 0; i < 3; ++i) {
		Ref<RNShadowNode> span;
		span.instantiate();
		span->view_name = "RCTVirtualText";
		span->tag = 10 + i;
		span->descriptor = registry.find("RCTVirtualText");
		if (i == 0) {
			span->props["maxFontSizeMultiplier"] = Variant();
		}
		if (i == 1) {
			span->props["allowFontScaling"] = false;
		}
		if (i == 2) {
			span->props["maxFontSizeMultiplier"] = 0;
		}
		Ref<RNShadowNode> raw;
		raw.instantiate();
		raw->view_name = "RCTRawText";
		raw->descriptor = registry.find("RCTRawText");
		raw->props["text"] = "word ";
		span->children.push_back(raw);
		text->children.push_back(span);
	}
	RNPreparedHostState prepared;
	RNHostContext context;
	context.font_scale = 2;
	REQUIRE(text->descriptor->prepare(*text.ptr(), prepared, error));
	REQUIRE(text->descriptor->resolve_resources(prepared, context, error));
	const auto document = std::static_pointer_cast<const RNTextDocument>(prepared.component_data);
	REQUIRE(document->runs.size() == 3);
	CHECK(document->runs[0].font.size == 30);
	CHECK(document->runs[1].font.size == 20);
	CHECK(document->runs[2].font.size == 40);
	RNMeasureConstraints constraints;
	constraints.width = 600;
	constraints.width_mode = RNMeasureMode::AT_MOST;
	const Size2 measured = text->descriptor->measure(prepared, constraints);
	auto native = memnew(RNTextControl);
	native->set_external_layout_enabled(true);
	native->apply_document(document);
	native->set_size(Size2(600, measured.y));
	REQUIRE(native->validate_detached_layout());
	CHECK(native->get_content_height() == measured.y);
	CHECK(native->get_line_height(0) == 36);
	memdelete(native);
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
TEST_CASE("[ReactNativeBindings][WebSocket] a full native queue retries open before delivering messages") {
	SocketFactoryGuard guard;
	auto runtime = HermesRuntimeSingleton::get_singleton();
	runtime->reset();
	const uint64_t generation = runtime->get_runtime_generation();
	auto registry = std::make_shared<RNNativeModuleRegistry>(std::shared_ptr<RNRuntimeCoordinatorState>());
	registry->begin_generation(generation);
	RNServiceSettings settings;
	RNError error;
	REQUIRE(RNServiceSettings::snapshot(settings, error));
	REQUIRE(rn_register_websocket_module(*registry, [settings] { return settings; }, [] { return std::shared_ptr<RNBlobService>(); }, error));
	runtime->install_host_object("__testSockets", registry);
	runtime->evaluate("globalThis.socketEvents=[];const ws=__testSockets.get('GodotWebSocket');ws.onEvent(event=>{if(event.name!=='blocked')socketEvents.push(event.name);});ws.connect('ws://localhost/test',[],{},123);undefined;");
	REQUIRE(runtime->get_last_error().is_empty());
	Dictionary payload;
	payload["name"] = "blocked";
	payload["payload"] = Dictionary();
	for (int i = 0; i < 1024; ++i) {
		REQUIRE(registry->queue_event("GodotWebSocket", "event", "", generation, payload));
	}
	registry->process_frame(0);
	CHECK(runtime->evaluate("JSON.stringify(socketEvents)") == Variant("[]"));
	for (int i = 0; i < 4; ++i) {
		runtime->dispatch_native_module_deliveries(registry);
	}
	registry->process_frame(1);
	runtime->dispatch_native_module_deliveries(registry);
	CHECK(runtime->evaluate("JSON.stringify(socketEvents)") == Variant("[\"websocketOpen\",\"websocketMessage\"]"));
	registry->process_frame(2);
	runtime->dispatch_native_module_deliveries(registry);
	CHECK(runtime->evaluate("socketEvents.length") == Variant(2));
	runtime->reset();
	runtime->uninstall_host_object("__testSockets");
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
TEST_CASE("[ReactNativeBindings][WebSocket] failed handshakes and inbound errors survive a full native queue once") {
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
	auto runtime = HermesRuntimeSingleton::get_singleton();
	runtime->reset();
	const uint64_t generation = runtime->get_runtime_generation();
	auto registry = std::make_shared<RNNativeModuleRegistry>(std::shared_ptr<RNRuntimeCoordinatorState>());
	registry->begin_generation(generation);
	RNServiceSettings settings;
	RNError error;
	REQUIRE(RNServiceSettings::snapshot(settings, error));
	REQUIRE(rn_register_websocket_module(*registry, [settings] { return settings; }, [] { return std::shared_ptr<RNBlobService>(); }, error));
	runtime->install_host_object("__testFailure", registry);
	runtime->evaluate("globalThis.failedEvents=[];globalThis.ws=__testFailure.get('GodotWebSocket');ws.onEvent(event=>{if(event.name!=='blocked')failedEvents.push(event.name);});ws.connect('ws://localhost/test',[],{},123);undefined;");
	REQUIRE(runtime->get_last_error().is_empty());
	Dictionary payload;
	payload["name"] = "blocked";
	payload["payload"] = Dictionary();
	for (int i = 0; i < 1024; ++i) {
		REQUIRE(registry->queue_event("GodotWebSocket", "event", "", generation, payload));
	}
	registry->process_frame(0);
	CHECK(runtime->evaluate("JSON.stringify(failedEvents)") == Variant("[]"));
	CHECK(runtime->evaluate("ws.stats().peers") == Variant(1));
	for (int i = 0; i < 4; ++i) {
		runtime->dispatch_native_module_deliveries(registry);
	}
	for (int i = 0; i < 3; ++i) {
		registry->process_frame(i + 1);
		runtime->dispatch_native_module_deliveries(registry);
	}
	CHECK(runtime->evaluate("failedEvents.filter(name=>name==='websocketFailed').length") == Variant(1));
	CHECK(runtime->evaluate("ws.stats().peers") == Variant(0));
	CHECK(runtime->evaluate("failedEvents.includes('websocketClosed')") == Variant(false));
	runtime->reset();
	runtime->uninstall_host_object("__testFailure");
}
TEST_CASE("[ReactNativeBindings][Services] blocked application events coalesce the final state without repeated native invalidation") {
	auto runtime = HermesRuntimeSingleton::get_singleton();
	runtime->reset();
	const uint64_t generation = runtime->get_runtime_generation();
	auto state = std::make_shared<RNRuntimeCoordinatorState>();
	auto registry = std::make_shared<RNNativeModuleRegistry>(state);
	registry->begin_generation(generation);
	RNError error;
	REQUIRE(rn_register_application_services(*registry, state, error));
	runtime->install_host_object("__testServices", registry);
	runtime->evaluate("globalThis.serviceEvents=[];globalThis.services=__testServices.get('GodotServices');services.getState();services.onEvent(event=>{if(event.name!=='blocked')serviceEvents.push(event);});undefined;");
	REQUIRE(runtime->get_last_error().is_empty());
	registry->process_frame(0);
	runtime->dispatch_native_module_deliveries(registry);
	runtime->evaluate("serviceEvents.length=0;undefined;");
	Dictionary payload;
	payload["name"] = "blocked";
	payload["payload"] = Dictionary();
	for (int i = 0; i < 1024; ++i) {
		REQUIRE(registry->queue_event("GodotServices", "event", "", generation, payload));
	}
	state->font_scale = 2;
	state->application_paused = true;
	runtime->evaluate("services.setColorScheme('dark');undefined;");
	state->font_scale = 3;
	runtime->evaluate("services.setColorScheme('light');undefined;");
	const uint64_t revision = state->metrics_revision;
	registry->process_frame(1);
	CHECK(state->metrics_revision == revision);
	CHECK(runtime->evaluate("serviceEvents.length") == Variant(0));
	for (int i = 0; i < 4; ++i) {
		runtime->dispatch_native_module_deliveries(registry);
	}
	registry->process_frame(2);
	runtime->dispatch_native_module_deliveries(registry);
	CHECK(runtime->evaluate("serviceEvents.length") == Variant(3));
	CHECK(runtime->evaluate("serviceEvents.find(event=>event.name==='didUpdateDimensions').payload.window.fontScale") == Variant(3));
	CHECK(runtime->evaluate("serviceEvents.find(event=>event.name==='appearanceChanged').payload.colorScheme") == Variant("light"));
	CHECK(runtime->evaluate("serviceEvents.find(event=>event.name==='appStateDidChange').payload.app_state") == Variant("background"));
	registry->process_frame(3);
	runtime->dispatch_native_module_deliveries(registry);
	CHECK(runtime->evaluate("serviceEvents.length") == Variant(3));
	runtime->reset();
	runtime->uninstall_host_object("__testServices");
}
struct SceneServiceFixture {
	HermesRuntimeSingleton *runtime = HermesRuntimeSingleton::get_singleton();
	ReactNativeRootView *root = memnew(ReactNativeRootView);
	std::shared_ptr<RNRuntimeCoordinatorState> state = std::make_shared<RNRuntimeCoordinatorState>();
	std::shared_ptr<RNNativeModuleRegistry> registry;
	uint64_t generation;
	SceneServiceFixture() {
		runtime->reset();
		generation = runtime->get_runtime_generation();
		RNSurfaceRoute route;
		route.root_tag = 11;
		route.root_view_id = root->get_instance_id();
		route.runtime_generation = generation;
		route.surface_epoch = 1;
		route.status = RNSurfaceStatus::ACTIVE;
		state->routes[11] = route;
		registry = std::make_shared<RNNativeModuleRegistry>(state);
		registry->begin_generation(generation);
		runtime->install_host_object("__testScene", registry);
	}
	~SceneServiceFixture() {
		runtime->reset();
		runtime->uninstall_host_object("__testScene");
		GodotAlerts::get_singleton()->configure(ReactNativeRuntimeCoordinator::get_singleton()->get_state());
		memdelete(root);
	}
};
TEST_CASE("[ReactNativeBindings][SceneBinding] full queues eventually resynchronize signals and replacement handles") {
	SceneServiceFixture fixture;
	RNError error;
	REQUIRE(rn_register_godot_scene_module(*fixture.registry, error));
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
	fixture.runtime->evaluate("globalThis.sceneEvents=[];globalThis.session=__testScene.openSession(11);const scene=__testScene.get('GodotScene');scene.onChanged(session,event=>{if(event.event!=='blocked')sceneEvents.push(event);});globalThis.before=scene.getBinding(session);undefined;");
	REQUIRE(fixture.runtime->get_last_error().is_empty());
	fixture.runtime->dispatch_native_module_deliveries(fixture.registry);
	fixture.runtime->evaluate("sceneEvents.length=0;undefined;");
	const String session = fixture.runtime->get_global("session");
	Dictionary payload;
	payload["event"] = "blocked";
	for (int i = 0; i < 1024; ++i) {
		REQUIRE(fixture.registry->queue_event("GodotScene", "changed", session, fixture.generation, payload));
	}
	target->emit_changed();
	fixture.registry->process_frame(0);
	CHECK(fixture.runtime->evaluate("sceneEvents.length") == Variant(0));
	for (int i = 0; i < 4; ++i) {
		fixture.runtime->dispatch_native_module_deliveries(fixture.registry);
	}
	fixture.registry->process_frame(1);
	fixture.runtime->dispatch_native_module_deliveries(fixture.registry);
	CHECK(fixture.runtime->evaluate("sceneEvents.length===1 && sceneEvents[0].event==='resync' && sceneEvents[0].sequence>before.sequence && sceneEvents[0].binding===before.binding") == Variant(true));
	for (int i = 0; i < 1024; ++i) {
		REQUIRE(fixture.registry->queue_event("GodotScene", "changed", session, fixture.generation, payload));
	}
	REQUIRE(fixture.root->attach_scene_binding(target.ptr(), resource).is_empty());
	fixture.registry->scene_binding_changed(fixture.root->get_instance_id());
	for (int i = 0; i < 4; ++i) {
		fixture.runtime->dispatch_native_module_deliveries(fixture.registry);
	}
	fixture.registry->process_frame(2);
	fixture.runtime->dispatch_native_module_deliveries(fixture.registry);
	CHECK(fixture.runtime->evaluate("sceneEvents.length===2 && sceneEvents[1].ready && sceneEvents[1].binding!==before.binding && sceneEvents[1].sequence>sceneEvents[0].sequence") == Variant(true));
	fixture.registry->process_frame(3);
	fixture.runtime->dispatch_native_module_deliveries(fixture.registry);
	CHECK(fixture.runtime->evaluate("sceneEvents.length") == Variant(2));
}
TEST_CASE("[ReactNativeBindings][Alerts] a full custom presentation queue rejects and removes the pending request") {
	SceneServiceFixture fixture;
	GodotAlerts::get_singleton()->configure(fixture.state);
	RNError error;
	REQUIRE(rn_register_alert_module(*fixture.registry, error));
	fixture.runtime->evaluate("globalThis.session=__testScene.openSession(11);globalThis.alerts=__testScene.get('GodotAlert');globalThis.origin=alerts.getOrigin(session);undefined;");
	REQUIRE(fixture.runtime->get_last_error().is_empty());
	for (int i = 0; i < 1024; ++i) {
		REQUIRE(fixture.registry->queue_event("GodotAlert", "present", "", fixture.generation, Dictionary()));
	}
	fixture.runtime->evaluate("globalThis.alertResult='pending';const request=alerts.__godotStartAsync('request',[origin,{title:'Test',message:'Message',buttons:[{text:'OK'}],cancelable:false},'custom',session]);globalThis.alertRequest=request.requestId;request.promise.then(()=>{alertResult='resolved';},error=>{alertResult=error.code;});undefined;");
	REQUIRE(fixture.runtime->get_last_error().is_empty());
	fixture.registry->process_jobs();
	for (int i = 0; i < 5; ++i) {
		fixture.runtime->dispatch_native_module_deliveries(fixture.registry);
	}
	CHECK(fixture.runtime->get_global("alertResult") == Variant(RNErrorCode::LIMIT));
	Dictionary result;
	result["buttonId"] = 0;
	result["dismissed"] = false;
	CHECK_FALSE(GodotAlerts::get_singleton()->reply(fixture.runtime->get_global("session"), fixture.runtime->get_global("alertRequest"), result, error));
	CHECK(error.code == RNErrorCode::CANCELLED);
}
} //namespace TestRNServices
void rn_force_link_service_tests() {}
