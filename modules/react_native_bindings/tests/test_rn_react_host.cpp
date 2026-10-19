#include "../host/rn_host_teardown.h"
#include "../host/rn_main_thread_queue.h"
#include "../mounting/rn_mounting_manager.h"
#include "../native_modules/rn_blob_service.h"
#include "../turbomodules/rn_godot_turbo_module_provider.h"
#include "support/rn_upstream_test_macros.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"

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

// Test runs have no project, so nothing has created the user data directory yet.
void write_script(const String &p_path, const String &p_source) {
	DirAccess::make_dir_recursive_absolute(OS::get_singleton()->get_user_data_dir());
	Ref<FileAccess> script = FileAccess::open(p_path, FileAccess::WRITE);
	REQUIRE(script.is_valid());
	script->store_string(p_source);
	script->close();
}

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
	write_script(script_path, "globalThis.__answer = 6 * 7;");

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

TEST_CASE("[ReactNativeBindings][ReactHost] each runtime generation installs its own LayoutAnimation driver") {
	const String script_path = "user://rn_layout_animation_test.js";
	write_script(script_path, "globalThis.__answer = 1;");
	auto main_thread = std::make_shared<RNMainThreadQueue>();
	auto registry_factory = [](const react::EventDispatcher::Weak &p_dispatcher, const std::shared_ptr<const react::ContextContainer> &p_context) {
		return react::ComponentDescriptorProviderRegistry().createComponentDescriptorRegistry({ p_dispatcher, p_context, nullptr });
	};
	std::shared_ptr<RNMountingManager> previous;
	for (int generation = 0; generation < 2; ++generation) {
		auto context = std::make_shared<react::ContextContainer>();
		context->insert(react::HttpClientFactoryKey, react::HttpClientFactory([] { return std::unique_ptr<react::IHttpClient>(); }));
		context->insert(react::WebSocketClientFactoryKey, react::WebSocketClientFactory([] { return std::unique_ptr<react::IWebSocketClient>(); }));
		auto mounting = std::make_shared<RNMountingManager>(main_thread, std::make_shared<RNComponentViewFactories>(), registry_factory, nullptr);
		react::ReactInstanceConfig config;
		config.enableDevMode = false;
		config.enableInspector = false;
		{
			react::ReactHost host(config, mounting, std::make_shared<react::RunLoopObserverManager>(), context, [](jsi::Runtime &, const react::JsErrorHandler::ProcessedError &) {}, nullptr);
			REQUIRE(host.loadScript(script_path.utf8().get_data(), ""));
			CHECK(mounting->has_layout_animations());
			mounting->tick_layout_animations();
		}
		CHECK(mounting != previous);
		previous = mounting;
	}
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
	const String source = String::utf8(
			"globalThis.setImmediate = callback => setTimeout(callback, 0);"
			"const constants = nativeModuleProxy.PlatformConstants.getConstants();"
			"globalThis.__minor = constants.reactNativeVersion.minor;"
			"const blobs = nativeModuleProxy.BlobModule;"
			"blobs.createFromParts([{type: 'string', data: 'héllo'}], 'turbo-blob');"
			"nativeModuleProxy.FileReaderModule.readAsText({blobId: 'turbo-blob', offset: 0, size: 6}, 'utf-8')"
			".then(text => { globalThis.__textLength = text === 'héllo' ? text.length : -1; blobs.release('turbo-blob'); }, () => { globalThis.__textLength = -2; });"
			"try { nativeModuleProxy.Appearance.setColorScheme('purple'); } catch (error) { globalThis.__rejected = error.code === 'E_VALIDATION' ? 1 : 0; }"
			"nativeModuleProxy.DevSettings.reloadWithReason('test');"
			"nativeModuleProxy.DevSettings.openDebugger();"
			"globalThis.__sourceMatches = nativeModuleProxy.SourceCode.getConstants().scriptURL === 'http://localhost:8081/index.bundle?platform=godot' ? 1 : 0;"
			"nativeModuleProxy.AccessibilityManager.getCurrentVoiceOverState(enabled => { globalThis.__screenReader = enabled ? 1 : 0; }, () => {});"
			"nativeModuleProxy.AccessibilityManager.getCurrentReduceMotionState(enabled => { globalThis.__reduceMotion = enabled ? 1 : 0; }, () => {});"
			"nativeModuleProxy.AccessibilityManager.setAccessibilityFocus(7);"
			"nativeModuleProxy.AccessibilityManager.announceForAccessibility('Saved');"
			"globalThis.__reloadRequested = 1;");
	write_script(script_path, source);

	auto main_thread = std::make_shared<RNMainThreadQueue>();
	auto blobs = std::make_shared<RNBlobService>(1024 * 1024);
	int reloads = 0;
	int debugger_requests = 0;
	int focused_tag = 0;
	react::RNTurboModuleContext modules;
	modules.host = { main_thread, nullptr, [&reloads] { ++reloads; }, [&debugger_requests] { ++debugger_requests; }, [&focused_tag](int p_tag) { focused_tag = p_tag; } };
	modules.source_url = "http://localhost:8081/index.bundle?platform=godot";
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
	auto source_matches = read_answer(host, "__sourceMatches");
	REQUIRE(source_matches.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
	CHECK(source_matches.get() == 1);
	auto screen_reader = read_answer(host, "__screenReader");
	REQUIRE(screen_reader.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
	CHECK(screen_reader.get() == 0);
	auto reduce_motion = read_answer(host, "__reduceMotion");
	REQUIRE(reduce_motion.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
	CHECK(reduce_motion.get() == 0);
	CHECK(reloads == 0);
	CHECK(debugger_requests == 0);
	CHECK(focused_tag == 0);
	main_thread->drain();
	CHECK(reloads == 1);
	CHECK(debugger_requests == 1);
	CHECK(focused_tag == 7);
}

void start_runtime(RNHostRuntime &r_runtime, const String &p_script_path, const String &p_source) {
	write_script(p_script_path, p_source);

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
