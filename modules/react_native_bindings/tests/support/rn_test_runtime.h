#pragma once

#include <hermes/hermes.h>
#include <jsi/instrumentation.h>
#include <jsi/jsi.h>

#include <memory>
#include <string>

// A standalone Hermes runtime for tests that only need to run JavaScript
// against native host objects, without a ReactHost.
class RNTestRuntime {
	std::unique_ptr<facebook::hermes::HermesRuntime> runtime = facebook::hermes::makeHermesRuntime();

public:
	operator facebook::jsi::Runtime &() { return *runtime; }
	facebook::jsi::Object global() { return runtime->global(); }
	facebook::jsi::Value evaluate(const std::string &p_code) {
		return runtime->evaluateJavaScript(std::make_shared<facebook::jsi::StringBuffer>(p_code), "test.js");
	}
	void collect_garbage() { runtime->instrumentation().collectGarbage("test"); }
};
