#pragma once

#include <ReactCommon/CallInvoker.h>
#include <jsi/jsi.h>

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

using RNJSValueFactory = std::function<facebook::jsi::Value(facebook::jsi::Runtime &)>;

facebook::jsi::Value rn_js_error(facebook::jsi::Runtime &p_runtime, const std::string &p_code, const std::string &p_message);
[[noreturn]] void rn_throw_js_error(facebook::jsi::Runtime &p_runtime, const std::string &p_code, const std::string &p_message);

// Settles a JS promise from any thread; the settlement runs on the JS thread.
class RNJSPromise : public std::enable_shared_from_this<RNJSPromise> {
	std::shared_ptr<facebook::react::CallInvoker> js_invoker;
	std::mutex mutex;
	std::optional<facebook::jsi::Function> resolve_function;
	std::optional<facebook::jsi::Function> reject_function;
	bool settled = false;

	bool _claim();

public:
	explicit RNJSPromise(std::shared_ptr<facebook::react::CallInvoker> p_js_invoker) :
			js_invoker(std::move(p_js_invoker)) {}

	static facebook::jsi::Value create(facebook::jsi::Runtime &p_runtime, std::shared_ptr<facebook::react::CallInvoker> p_js_invoker, const std::function<void(std::shared_ptr<RNJSPromise>)> &p_start);

	void resolve(RNJSValueFactory p_value);
	void reject(const std::string &p_code, const std::string &p_message);
};
