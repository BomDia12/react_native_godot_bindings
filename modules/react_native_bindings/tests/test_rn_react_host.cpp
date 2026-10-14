#include "../host/rn_main_thread_queue.h"
#include "../native_modules/rn_blob_service.h"
#include "../turbomodules/rn_godot_turbo_module_provider.h"
#include "support/rn_upstream_test_macros.h"

#include "core/io/file_access.h"

#include <react/http/IHttpClient.h>
#include <react/http/IWebSocketClient.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/uimanager/IMountingManager.h>
#include <react/runtime/ReactHost.h>
#include <react/utils/RunLoopObserverManager.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

namespace TestRNReactHost {

using namespace facebook;

class RecordingMountingManager : public react::IMountingManager {
public:
	std::atomic<int> transactions = 0;

	void executeMount(react::SurfaceId, react::MountingTransaction &&) override { ++transactions; }
	void dispatchCommand(const react::ShadowView &, const std::string &, const folly::dynamic &) override {}

	react::ComponentRegistryFactory getComponentRegistryFactory() override {
		return [](const react::EventDispatcher::Weak &p_dispatcher, const std::shared_ptr<const react::ContextContainer> &p_context) {
			return react::ComponentDescriptorProviderRegistry().createComponentDescriptorRegistry({ p_dispatcher, p_context, nullptr });
		};
	}
};

TEST_CASE("[ReactNativeBindings][ReactHost] upstream ReactHost evaluates a script on its JS thread") {
	const String script_path = "user://rn_react_host_test.js";
	Ref<FileAccess> script = FileAccess::open(script_path, FileAccess::WRITE);
	REQUIRE(script.is_valid());
	script->store_string("globalThis.__answer = 6 * 7;");
	script->close();

	auto context = std::make_shared<react::ContextContainer>();
	context->insert(react::HttpClientFactoryKey, react::HttpClientFactory([] { return std::unique_ptr<react::IHttpClient>(); }));
	context->insert(react::WebSocketClientFactoryKey, react::WebSocketClientFactory([] { return std::unique_ptr<react::IWebSocketClient>(); }));
	auto mounting = std::make_shared<RecordingMountingManager>();

	react::ReactInstanceConfig config;
	config.enableDevMode = false;
	config.enableInspector = false;
	react::ReactHost host(config, mounting, std::make_shared<react::RunLoopObserverManager>(), context, [](jsi::Runtime &, const react::JsErrorHandler::ProcessedError &) {}, nullptr);
	REQUIRE(host.loadScript(script_path.utf8().get_data(), ""));

	std::promise<double> answer;
	host.runOnRuntimeScheduler([&answer](jsi::Runtime &p_runtime) {
		answer.set_value(p_runtime.global().getProperty(p_runtime, "__answer").asNumber());
	});
	std::future<double> result = answer.get_future();
	REQUIRE(result.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
	CHECK(result.get() == 42);
	CHECK(mounting->transactions == 0);
}

std::future<double> read_answer(react::ReactHost &p_host, const std::string &p_global) {
	auto answer = std::make_shared<std::promise<double>>();
	p_host.runOnRuntimeScheduler([answer, p_global](jsi::Runtime &p_runtime) {
		const jsi::Value value = p_runtime.global().getProperty(p_runtime, p_global.c_str());
		answer->set_value(value.isNumber() ? value.asNumber() : -1);
	});
	return answer->get_future();
}

TEST_CASE("[ReactNativeBindings][ReactHost] Godot TurboModules serve JavaScript through the provider") {
	const String script_path = "user://rn_turbo_module_test.js";
	Ref<FileAccess> script = FileAccess::open(script_path, FileAccess::WRITE);
	REQUIRE(script.is_valid());
	script->store_string(String::utf8(
			"globalThis.setImmediate = callback => setTimeout(callback, 0);"
			"const constants = nativeModuleProxy.PlatformConstants.getConstants();"
			"globalThis.__minor = constants.reactNativeVersion.minor;"
			"const blobs = nativeModuleProxy.BlobModule;"
			"blobs.createFromParts([{type: 'string', data: 'héllo'}], 'turbo-blob');"
			"nativeModuleProxy.FileReaderModule.readAsText({blobId: 'turbo-blob', offset: 0, size: 6}, 'utf-8')"
			".then(text => { globalThis.__textLength = text === 'héllo' ? text.length : -1; blobs.release('turbo-blob'); }, () => { globalThis.__textLength = -2; });"
			"try { nativeModuleProxy.Appearance.setColorScheme('purple'); } catch (error) { globalThis.__rejected = error.code === 'E_VALIDATION' ? 1 : 0; }"));
	script->close();

	auto main_thread = std::make_shared<RNMainThreadQueue>();
	auto blobs = std::make_shared<RNBlobService>(1024 * 1024);
	react::RNTurboModuleContext modules;
	modules.application = { std::make_shared<RNGodotApplication>(), main_thread };
	modules.network = { blobs, nullptr, nullptr, main_thread };
	auto context = std::make_shared<react::ContextContainer>();
	context->insert(react::HttpClientFactoryKey, react::HttpClientFactory([] { return std::unique_ptr<react::IHttpClient>(); }));
	context->insert(react::WebSocketClientFactoryKey, react::WebSocketClientFactory([] { return std::unique_ptr<react::IWebSocketClient>(); }));
	react::ReactInstanceConfig config;
	config.enableDevMode = false;
	config.enableInspector = false;
	react::ReactHost host(config, std::make_shared<RecordingMountingManager>(), std::make_shared<react::RunLoopObserverManager>(), context, [](jsi::Runtime &, const react::JsErrorHandler::ProcessedError &) {}, nullptr, nullptr, { react::rn_godot_turbo_module_provider(modules) });
	REQUIRE(host.loadScript(script_path.utf8().get_data(), ""));

	auto minor = read_answer(host, "__minor");
	REQUIRE(minor.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
	CHECK(minor.get() == 87);
	auto rejected = read_answer(host, "__rejected");
	REQUIRE(rejected.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
	CHECK(rejected.get() == 1);
	double length = -1;
	for (int attempt = 0; attempt < 100 && length < 0; ++attempt) {
		auto value = read_answer(host, "__textLength");
		REQUIRE(value.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
		length = value.get();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	CHECK(length == 5);
	CHECK(blobs->count() == 0);
}

} // namespace TestRNReactHost

void rn_force_link_react_host_tests() {}
