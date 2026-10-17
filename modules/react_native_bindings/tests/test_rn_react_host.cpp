#include "../host/rn_host_teardown.h"
#include "../host/rn_main_thread_queue.h"
#include "../native_modules/rn_blob_service.h"
#include "../turbomodules/rn_godot_turbo_module_provider.h"
#include "support/rn_upstream_test_macros.h"

#include "core/io/file_access.h"

#include <jsi/hermes-interfaces.h>
#include <react/http/IHttpClient.h>
#include <react/http/IWebSocketClient.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/uimanager/IMountingManager.h>
#include <react/runtime/ReactHost.h>
#include <react/runtime/platform/godot/GodotTimerRegistry.h>
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
			"try { nativeModuleProxy.Appearance.setColorScheme('purple'); } catch (error) { globalThis.__rejected = error.code === 'E_VALIDATION' ? 1 : 0; }"
			"nativeModuleProxy.DevSettings.reloadWithReason('test');"
			"globalThis.__reloadRequested = 1;"));
	script->close();

	auto main_thread = std::make_shared<RNMainThreadQueue>();
	auto blobs = std::make_shared<RNBlobService>(1024 * 1024);
	int reloads = 0;
	react::RNTurboModuleContext modules;
	modules.host = { main_thread, nullptr, [&reloads] { ++reloads; } };
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
	auto reload_requested = read_answer(host, "__reloadRequested");
	REQUIRE(reload_requested.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
	CHECK(reload_requested.get() == 1);
	CHECK(reloads == 0);
	main_thread->drain();
	CHECK(reloads == 1);
}

void start_runtime(RNHostRuntime &r_runtime, const String &p_script_path, const String &p_source) {
	Ref<FileAccess> script = FileAccess::open(p_script_path, FileAccess::WRITE);
	REQUIRE(script.is_valid());
	script->store_string(p_source);
	script->close();

	auto context = std::make_shared<react::ContextContainer>();
	context->insert(react::HttpClientFactoryKey, react::HttpClientFactory([] { return std::unique_ptr<react::IHttpClient>(); }));
	context->insert(react::WebSocketClientFactoryKey, react::WebSocketClientFactory([] { return std::unique_ptr<react::IWebSocketClient>(); }));
	react::ReactInstanceConfig config;
	config.enableDevMode = false;
	config.enableInspector = false;
	config.platformTimerRegistryFactory = [&r_runtime, context] {
		auto registry = std::make_unique<react::GodotTimerRegistry>(context);
		r_runtime.timers = registry.get();
		return std::unique_ptr<react::PlatformTimerRegistry>(std::move(registry));
	};
	auto install_bindings = [&r_runtime](jsi::Runtime &p_runtime) {
		r_runtime.hermes = jsi::castInterface<facebook::hermes::IHermes>(&p_runtime);
	};
	r_runtime.react_host = std::make_unique<react::ReactHost>(config, std::make_shared<RecordingMountingManager>(), std::make_shared<react::RunLoopObserverManager>(), context, [](jsi::Runtime &, const react::JsErrorHandler::ProcessedError &) {}, nullptr, nullptr, react::TurboModuleProviders{}, nullptr, nullptr, install_bindings);
	REQUIRE(r_runtime.react_host->loadScript(p_script_path.utf8().get_data(), ""));
}

void busy_loop(jsi::Runtime &p_runtime, int p_milliseconds) {
	try {
		p_runtime.evaluateJavaScript(std::make_shared<jsi::StringBuffer>("const end = Date.now() + " + std::to_string(p_milliseconds) + "; while (Date.now() < end) {}"), "busy.js");
	} catch (const jsi::JSIException &) {
	}
}

TEST_CASE("[ReactNativeBindings][ReactHost] interval ticks do not pile up while JavaScript is busy") {
	RNHostRuntime runtime;
	start_runtime(runtime, "user://rn_timer_registry_test.js", "globalThis.__ticks = 0; setInterval(() => { globalThis.__ticks++; }, 0);");
	double ticks = 0;
	for (int attempt = 0; attempt < 100 && ticks < 1; ++attempt) {
		auto value = read_answer(*runtime.react_host, "__ticks");
		REQUIRE(value.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
		ticks = value.get();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	REQUIRE(ticks >= 1);

	auto before = std::make_shared<std::promise<double>>();
	runtime.react_host->runOnRuntimeScheduler([&runtime, before](jsi::Runtime &p_runtime) {
		before->set_value(p_runtime.global().getProperty(p_runtime, "__ticks").asNumber());
		busy_loop(p_runtime, 500);
		runtime.timers->quit();
	});
	auto after = read_answer(*runtime.react_host, "__ticks");
	REQUIRE(after.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
	CHECK(after.get() - before->get_future().get() <= 2);
}

TEST_CASE("[ReactNativeBindings][ReactHost] teardown interrupts busy JavaScript off the calling thread") {
	RNHostRuntime runtime;
	start_runtime(runtime, "user://rn_teardown_test.js", "setInterval(() => {}, 0); globalThis.__loaded = 1;");
	auto loaded = read_answer(*runtime.react_host, "__loaded");
	REQUIRE(loaded.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
	REQUIRE(runtime.hermes.load() != nullptr);

	auto busy = std::make_shared<std::promise<void>>();
	runtime.react_host->runOnRuntimeScheduler([busy](jsi::Runtime &p_runtime) {
		busy->set_value();
		busy_loop(p_runtime, 3000);
	});
	busy->get_future().wait();

	const auto started = std::chrono::steady_clock::now();
	RNHostTeardown teardown(runtime, {});
	CHECK(std::chrono::steady_clock::now() - started < std::chrono::milliseconds(50));
	while (!teardown.is_finished() && std::chrono::steady_clock::now() - started < std::chrono::milliseconds(2500)) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	CHECK(teardown.is_finished());
	CHECK(runtime.react_host == nullptr);
}

} // namespace TestRNReactHost

void rn_force_link_react_host_tests() {}
