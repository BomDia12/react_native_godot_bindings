#include "core/io/file_access.h"
#include "tests/test_macros.h"

#include <react/http/IHttpClient.h>
#include <react/http/IWebSocketClient.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/uimanager/IMountingManager.h>
#include <react/runtime/ReactHost.h>
#include <react/utils/RunLoopObserverManager.h>

#include <atomic>
#include <chrono>
#include <future>

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

} // namespace TestRNReactHost

void rn_force_link_react_host_tests() {}
