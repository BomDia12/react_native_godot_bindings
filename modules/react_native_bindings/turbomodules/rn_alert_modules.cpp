#include "rn_alert_modules.h"

#include "../host/rn_main_thread_queue.h"
#include "../interop/rn_dynamic.h"
#include "rn_godot_alerts.h"
#include "rn_js_promise.h"

#include <jsi/JSIDynamic.h>

namespace facebook::react {

namespace {

Dictionary alert_result(const folly::dynamic &p_result) {
	Dictionary result;
	const folly::dynamic button = p_result.getDefault("buttonId", nullptr);
	result["buttonId"] = button.isNumber() ? Variant(int64_t(button.asDouble())) : Variant();
	result["dismissed"] = p_result.getDefault("dismissed", false).asBool();
	return result;
}

} // namespace

RNGodotAlertModule::RNGodotAlertModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_context) :
		NativeGodotAlertCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
}

jsi::Value RNGodotAlertModule::request(jsi::Runtime &rt, double rootTag, jsi::Object payload, const std::string &mode) {
	const Dictionary request = rn_variant_from_dynamic(jsi::dynamicFromValue(rt, jsi::Value(rt, payload)));
	const String presentation = String::utf8(mode.c_str());
	const int root_tag = int(rootTag);
	auto main_thread = context.main_thread;
	auto find_root = context.find_root;
	return RNJSPromise::create(rt, jsInvoker_, [=](std::shared_ptr<RNJSPromise> p_promise) {
		main_thread->post([=] {
			GodotAlerts::get_singleton()->request(find_root(root_tag), root_tag, request, presentation, [p_promise](const Dictionary &p_result, const RNError &p_error) {
				if (p_error.is_set()) {
					p_promise->reject(p_error.code.utf8().get_data(), p_error.message.utf8().get_data());
					return;
				}
				const folly::dynamic result = rn_dynamic_from_variant(p_result);
				p_promise->resolve([result](jsi::Runtime &p_runtime) { return jsi::valueFromDynamic(p_runtime, result); });
			});
		});
	});
}

void RNGodotAlertModule::reply(jsi::Runtime &rt, double rootTag, const std::string &requestId, jsi::Object result) {
	const Dictionary settled = alert_result(jsi::dynamicFromValue(rt, jsi::Value(rt, result)));
	context.main_thread->post([root_tag = int(rootTag), request = String::utf8(requestId.c_str()), settled] {
		RNError error;
		if (!GodotAlerts::get_singleton()->reply(root_tag, request, settled, error)) {
			ERR_PRINT(error.describe());
		}
	});
}

void RNGodotAlertModule::decline(jsi::Runtime &rt, double rootTag, const std::string &requestId) {
	context.main_thread->post([root_tag = int(rootTag), request = String::utf8(requestId.c_str())] {
		RNError error;
		if (!GodotAlerts::get_singleton()->decline(root_tag, request, error)) {
			ERR_PRINT(error.describe());
		}
	});
}

void RNGodotAlertModule::present(const std::string &p_request, int p_root_tag, folly::dynamic p_payload) {
	const folly::dynamic presentation = folly::dynamic::object("requestId", p_request)("rootTag", p_root_tag)("payload", std::move(p_payload));
	static_cast<AsyncEventEmitter<jsi::Value> &>(*eventEmitterMap_["onPresent"]).emit([presentation](jsi::Runtime &p_runtime) { return jsi::valueFromDynamic(p_runtime, presentation); });
}

RNAlertManagerModule::RNAlertManagerModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_context) :
		NativeAlertManagerCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
}

void RNAlertManagerModule::alertWithArgs(jsi::Runtime &rt, jsi::Object args, AsyncCallback<double, std::string> callback) {
	const folly::dynamic arguments = jsi::dynamicFromValue(rt, jsi::Value(rt, args));
	const std::string cancel_key = arguments.getDefault("cancelButtonKey", "").asString();
	const std::string destructive_key = arguments.getDefault("destructiveButtonKey", "").asString();
	Array buttons;
	std::vector<std::string> keys;
	for (const folly::dynamic &button : arguments.getDefault("buttons", folly::dynamic::array())) {
		for (const auto &[key, text] : button.items()) {
			Dictionary entry;
			entry["text"] = String::utf8(text.asString().c_str());
			entry["style"] = key.asString() == cancel_key ? "cancel" : key.asString() == destructive_key ? "destructive"
																										 : "default";
			buttons.push_back(entry);
			keys.push_back(key.asString());
		}
	}
	if (buttons.is_empty()) {
		Dictionary ok;
		ok["text"] = "OK";
		ok["style"] = "default";
		buttons.push_back(ok);
		keys.push_back("0");
	}
	Dictionary payload;
	payload["title"] = String::utf8(arguments.getDefault("title", "").asString().c_str());
	payload["message"] = String::utf8(arguments.getDefault("message", "").asString().c_str());
	payload["cancelable"] = false;
	payload["buttons"] = buttons;
	context.main_thread->post([payload, keys, callback = std::move(callback)] {
		GodotAlerts::get_singleton()->request(nullptr, 0, payload, "bubble", [keys, callback](const Dictionary &p_result, const RNError &p_error) {
			const Variant button = p_result.get("buttonId", Variant());
			if (p_error.is_set() || button.get_type() != Variant::INT) {
				return;
			}
			const int index = int(button);
			callback(double(std::stoi(keys[index])), keys[index]);
		});
	});
}

} // namespace facebook::react
