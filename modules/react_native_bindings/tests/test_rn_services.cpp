#include "../components/rn_builtin_descriptors.h"
#include "../components/rn_font.h"
#include "../components/rn_text_control.h"
#include "../fabric/rn_shadow_node.h"
#include "../interop/rn_scene_binding.h"
#include "../native_modules/rn_blob_service.h"
#include "../native_modules/rn_cookie_jar.h"
#include "../runtime/rn_service_settings.h"
#include "../singletons/hermes_runtime_singleton.h"

#include "core/config/project_settings.h"
#include "scene/2d/node_2d.h"
#include "tests/test_macros.h"

namespace TestRNServices {
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
} //namespace TestRNServices
void rn_force_link_service_tests() {}
