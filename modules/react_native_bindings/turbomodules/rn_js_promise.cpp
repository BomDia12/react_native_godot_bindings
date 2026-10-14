#include "rn_js_promise.h"

using namespace facebook;

jsi::Value rn_js_error(jsi::Runtime &p_runtime, const std::string &p_code, const std::string &p_message) {
	jsi::Object error = p_runtime.global().getPropertyAsFunction(p_runtime, "Error").callAsConstructor(p_runtime, p_message).asObject(p_runtime);
	error.setProperty(p_runtime, "code", p_code);
	return error;
}

void rn_throw_js_error(jsi::Runtime &p_runtime, const std::string &p_code, const std::string &p_message) {
	throw jsi::JSError(p_runtime, rn_js_error(p_runtime, p_code, p_message));
}

jsi::Value RNJSPromise::create(jsi::Runtime &p_runtime, std::shared_ptr<react::CallInvoker> p_js_invoker, const std::function<void(std::shared_ptr<RNJSPromise>)> &p_start) {
	auto promise = std::make_shared<RNJSPromise>(std::move(p_js_invoker));
	auto executor = jsi::Function::createFromHostFunction(p_runtime, jsi::PropNameID::forAscii(p_runtime, "executor"), 2, [promise](jsi::Runtime &p_rt, const jsi::Value &, const jsi::Value *p_args, size_t) {
		promise->resolve_function = p_args[0].asObject(p_rt).asFunction(p_rt);
		promise->reject_function = p_args[1].asObject(p_rt).asFunction(p_rt);
		return jsi::Value::undefined();
	});
	jsi::Value result = p_runtime.global().getPropertyAsFunction(p_runtime, "Promise").callAsConstructor(p_runtime, executor);
	p_start(promise);
	return result;
}

bool RNJSPromise::_claim() {
	std::lock_guard lock(mutex);
	if (settled) {
		return false;
	}
	settled = true;
	return true;
}

void RNJSPromise::resolve(RNJSValueFactory p_value) {
	if (!_claim()) {
		return;
	}
	js_invoker->invokeAsync([self = shared_from_this(), value = std::move(p_value)](jsi::Runtime &p_runtime) {
		self->resolve_function->call(p_runtime, value(p_runtime));
		self->resolve_function.reset();
		self->reject_function.reset();
	});
}

void RNJSPromise::reject(const std::string &p_code, const std::string &p_message) {
	if (!_claim()) {
		return;
	}
	js_invoker->invokeAsync([self = shared_from_this(), p_code, p_message](jsi::Runtime &p_runtime) {
		self->reject_function->call(p_runtime, rn_js_error(p_runtime, p_code, p_message));
		self->resolve_function.reset();
		self->reject_function.reset();
	});
}
