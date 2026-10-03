#include "rn_alert_service.h"

#include "../root_view/react_native_root_view.h"
#include "../runtime/react_native_runtime_coordinator.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/gui/dialogs.h"
#include "scene/main/scene_tree.h"

GodotAlerts *GodotAlerts::singleton = nullptr;
GodotAlerts::GodotAlerts() {
	singleton = this;
}
GodotAlerts::~GodotAlerts() {
	shutdown();
	singleton = nullptr;
}
void GodotAlerts::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_alert_handler", "handler"), &GodotAlerts::set_alert_handler);
	ClassDB::bind_method(D_METHOD("complete_alert", "request", "result"), &GodotAlerts::complete_alert);
}
bool GodotAlerts::validate_origin(const Variant &p_origin, RNExecutionOrigin &r_origin, ObjectID &r_root, RNError &r_error) const {
	if (p_origin.get_type() == Variant::NIL) {
		return true;
	}
	if (p_origin.get_type() != Variant::DICTIONARY) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "origin must be a record or null", "GodotAlerts.origin");
		return false;
	}
	auto shared = state.lock();
	const Dictionary origin = p_origin;
	for (const char *key : { "generation", "rootTag", "epoch" }) {
		if (!origin.has(key) || origin[key].get_type() != Variant::INT) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "origin requires integer generation/rootTag/epoch", "GodotAlerts.origin");
			return false;
		}
	}
	r_origin = { uint64_t(int64_t(origin["generation"])), int(origin["rootTag"]), uint64_t(int64_t(origin["epoch"])) };
	auto route = shared ? shared->routes.find(r_origin.root_tag) : decltype(shared->routes)::iterator();
	if (!shared || route == shared->routes.end() || route->second.runtime_generation != r_origin.generation || route->second.surface_epoch != r_origin.surface_epoch || !ObjectDB::get_instance(route->second.root_view_id)) {
		r_error = RNError::make(RNErrorCode::CANCELLED, "alert origin is stale", "GodotAlerts.origin");
		return false;
	}
	r_root = route->second.root_view_id;
	return true;
}
void GodotAlerts::discard(const String &p_request, const RNError &p_error) {
	Request *pending = requests.getptr(p_request);
	if (!pending) {
		return;
	}
	const Request request = *pending;
	requests.erase(p_request);
	if (auto dialog = Object::cast_to<Node>(ObjectDB::get_instance(request.dialog))) {
		dialog->queue_free();
	}
	request.completion.fail(p_error);
}
bool GodotAlerts::bubble(const String &p_request, RNError &r_error) {
	Request *pending = requests.getptr(p_request);
	if (!pending) {
		return false;
	}
	if (pending->bubbled) {
		r_error = RNError::make(RNErrorCode::CANCELLED, "alert has already bubbled", "GodotAlerts.reply");
		return false;
	}
	pending->bubbled = true;
	auto root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(pending->owner));
	Callable selected = root ? root->get_alert_handler() : Callable();
	if (!selected.is_valid()) {
		selected = handler;
		pending->owner = get_instance_id();
	}
	if (!selected.is_valid()) {
		r_error = RNError::make(RNErrorCode::UNSUPPORTED, "no Godot alert handler is installed", "GodotAlerts.request");
		return false;
	}
	Variant origin;
	if (pending->origin.root_tag) {
		Dictionary value;
		value["generation"] = int64_t(pending->origin.generation);
		value["rootTag"] = pending->origin.root_tag;
		value["epoch"] = int64_t(pending->origin.surface_epoch);
		origin = value;
	}
	const Dictionary payload = pending->payload.duplicate(true);
	selected.call_deferred(p_request, origin, payload);
	return true;
}
void GodotAlerts::request(const Variant &p_origin, const Dictionary &p_payload, const String &p_mode, const String &p_session, const RNCallContext &p_context, const RNCompletionToken &p_completion) {
	RNError error;
	Request pending;
	pending.completion = p_completion;
	pending.session = p_session;
	pending.payload = p_payload.duplicate(true);
	if (!validate_origin(p_origin, pending.origin, pending.owner, error)) {
		p_completion.fail(error);
		return;
	}
	if (!p_session.is_empty()) {
		RNSessionRecord session;
		if (!p_context.objects->resolve_session(p_session, session, error) || session.generation != pending.origin.generation || session.root_tag != pending.origin.root_tag || session.surface_epoch != pending.origin.surface_epoch) {
			p_completion.fail(error.is_set() ? error : RNError::make(RNErrorCode::CANCELLED, "alert session and origin differ", "GodotAlerts.request"));
			return;
		}
	} else if (p_mode != "bubble") {
		p_completion.fail(RNError::make(RNErrorCode::VALIDATION, "local presentation requires a session", "GodotAlerts.request"));
		return;
	}
	if (requests.size() >= 128 || !p_payload.has("buttons") || p_payload["buttons"].get_type() != Variant::ARRAY || Array(p_payload["buttons"]).size() < 1 || Array(p_payload["buttons"]).size() > 64 || p_payload.get("title", Variant()).get_type() != Variant::STRING || p_payload.get("message", Variant()).get_type() != Variant::STRING || p_payload.get("cancelable", Variant()).get_type() != Variant::BOOL) {
		p_completion.fail(RNError::make(RNErrorCode::VALIDATION, "invalid or oversized alert request", "GodotAlerts.request"));
		return;
	}
	const Array buttons = p_payload["buttons"];
	for (int i = 0; i < buttons.size(); ++i) {
		if (buttons[i].get_type() != Variant::DICTIONARY || Dictionary(buttons[i]).get("text", Variant()).get_type() != Variant::STRING) {
			p_completion.fail(RNError::make(RNErrorCode::VALIDATION, "button requires a text label", "GodotAlerts.request"));
			return;
		}
	}
	if (p_mode != "bubble" && p_mode != "native" && p_mode != "custom") {
		p_completion.fail(RNError::make(RNErrorCode::VALIDATION, "unknown alert mode", "GodotAlerts.request"));
		return;
	}
	if (!pending.origin.root_tag && p_mode != "bubble") {
		p_completion.fail(RNError::make(RNErrorCode::VALIDATION, "local presentation requires a root", "GodotAlerts.request"));
		return;
	}
	requests[p_context.request_token] = pending;
	if (p_mode == "bubble") {
		if (!bubble(p_context.request_token, error)) {
			discard(p_context.request_token, error);
		}
	} else if (p_mode == "custom") {
		Dictionary event;
		event["requestId"] = p_context.request_token;
		event["rootTag"] = pending.origin.root_tag;
		event["payload"] = pending.payload;
		p_completion.emit("present", event);
	} else {
		auto root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(pending.owner));
		if (!root || !root->is_inside_tree()) {
			discard(p_context.request_token, RNError::make(RNErrorCode::CANCELLED, "alert root left the scene", "GodotAlerts.request"));
			return;
		}
		AcceptDialog *dialog = memnew(AcceptDialog);
		dialog->set_title(p_payload["title"]);
		dialog->set_text(p_payload["message"]);
		dialog->set_close_on_escape(bool(p_payload["cancelable"]));
		dialog->get_ok_button()->set_text(Dictionary(buttons[0])["text"]);
		dialog->connect("confirmed", callable_mp(this, &GodotAlerts::_selected).bind(p_context.request_token, 0));
		for (int i = 1; i < buttons.size(); ++i) {
			Button *button = dialog->add_button(Dictionary(buttons[i])["text"], false, String::num_int64(i));
			button->connect("pressed", callable_mp(this, &GodotAlerts::_selected).bind(p_context.request_token, i));
		}
		dialog->connect("canceled", callable_mp(this, &GodotAlerts::_dismissed).bind(p_context.request_token));
		requests[p_context.request_token].dialog = dialog->get_instance_id();
		root->add_child(dialog);
		dialog->popup_centered();
	}
}
bool GodotAlerts::complete_for(ObjectID p_owner, const String &p_request, const Dictionary &p_result, RNError &r_error) {
	Request *pending = requests.getptr(p_request);
	if (!pending || pending->owner != p_owner) {
		r_error = RNError::make(RNErrorCode::CANCELLED, "request belongs to another handler or is closed", "GodotAlerts.complete");
		return false;
	}
	if (p_result.size() != 2 || !p_result.has("buttonId") || p_result.get("dismissed", Variant()).get_type() != Variant::BOOL) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "result requires buttonId and dismissed", "GodotAlerts.complete");
		return false;
	}
	const bool dismissed = p_result["dismissed"];
	const Variant index = p_result["buttonId"];
	if ((dismissed && index.get_type() != Variant::NIL) || (!dismissed && (index.get_type() != Variant::INT || int64_t(index) < 0 || int64_t(index) >= Array(pending->payload["buttons"]).size()))) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "invalid alert button result", "GodotAlerts.complete");
		return false;
	}
	const Request settled = *pending;
	requests.erase(p_request);
	if (auto dialog = Object::cast_to<Node>(ObjectDB::get_instance(settled.dialog))) {
		dialog->queue_free();
	}
	settled.completion.complete(p_result);
	return true;
}
bool GodotAlerts::complete_alert(const String &p_request, const Dictionary &p_result) {
	RNError error;
	return complete_for(get_instance_id(), p_request, p_result, error);
}
bool GodotAlerts::reply(const String &p_session, const String &p_request, const Dictionary &p_result, RNError &r_error) {
	Request *pending = requests.getptr(p_request);
	if (!pending || pending->session != p_session || p_session.is_empty()) {
		r_error = RNError::make(RNErrorCode::CANCELLED, "custom presenter session is stale", "GodotAlerts.reply");
		return false;
	}
	if (p_result.size() == 1 && p_result.get("handled", Variant()).get_type() == Variant::BOOL && !bool(p_result["handled"])) {
		if (!bubble(p_request, r_error)) {
			discard(p_request, r_error);
			return false;
		}
		return true;
	}
	return complete_for(pending->owner, p_request, p_result, r_error);
}
void GodotAlerts::_selected(const String &p_request, int p_button) {
	Request *pending = requests.getptr(p_request);
	if (!pending) {
		return;
	}
	Dictionary result;
	result["buttonId"] = p_button;
	result["dismissed"] = false;
	RNError error;
	complete_for(pending->owner, p_request, result, error);
}
void GodotAlerts::_dismissed(const String &p_request) {
	Request *pending = requests.getptr(p_request);
	if (!pending || !bool(pending->payload["cancelable"])) {
		return;
	}
	Dictionary result;
	result["buttonId"] = Variant();
	result["dismissed"] = true;
	RNError error;
	complete_for(pending->owner, p_request, result, error);
}
void GodotAlerts::cancel(const String &p_request) {
	discard(p_request, RNError::make(RNErrorCode::CANCELLED, "alert was canceled", "GodotAlerts"));
}
void GodotAlerts::close_session(const String &p_session) {
	Vector<String> closed;
	for (const auto &entry : requests) {
		if (entry.value.session == p_session) {
			closed.push_back(entry.key);
		}
	}
	for (const String &id : closed) {
		cancel(id);
	}
}
void GodotAlerts::close_surface(int p_tag, uint64_t p_epoch) {
	Vector<String> closed;
	for (const auto &entry : requests) {
		if (entry.value.origin.root_tag == p_tag && entry.value.origin.surface_epoch == p_epoch) {
			closed.push_back(entry.key);
		}
	}
	for (const String &id : closed) {
		cancel(id);
	}
}
void GodotAlerts::shutdown() {
	Vector<String> closed;
	for (const auto &entry : requests) {
		closed.push_back(entry.key);
	}
	for (const String &id : closed) {
		cancel(id);
	}
}

namespace {
class RNAlertModule : public RNNativeModule {
public:
	RNModuleResult invoke_sync(const StringName &p_method, const Array &p_args, const RNCallContext &p_context) override {
		RNError error;
		if (p_method == "getOrigin") {
			RNSessionRecord session;
			if (!p_context.objects->resolve_session(p_args[0], session, error)) {
				return RNModuleResult::failure(error);
			}
			Dictionary origin;
			origin["generation"] = int64_t(session.generation);
			origin["rootTag"] = session.root_tag;
			origin["epoch"] = int64_t(session.surface_epoch);
			return RNModuleResult::success(origin);
		}
		if (p_method == "reply") {
			if (!GodotAlerts::get_singleton()->reply(p_args[0], p_args[1], p_args[2], error)) {
				return RNModuleResult::failure(error);
			}
			return RNModuleResult::success();
		}
		return RNModuleResult::failure(RNError::make(RNErrorCode::UNSUPPORTED, "unknown alert operation", "GodotAlerts"));
	}
	void start_async(const StringName &, const Array &p_args, const RNCallContext &p_context, const RNCompletionToken &p_completion) override { GodotAlerts::get_singleton()->request(p_args[0], p_args[1], p_args[2], p_args[3], p_context, p_completion); }
	void on_session_closed(const String &p_session) override { GodotAlerts::get_singleton()->close_session(p_session); }
	void on_surface_closed(int p_tag, uint64_t p_epoch) override { GodotAlerts::get_singleton()->close_surface(p_tag, p_epoch); }
	void cancel(const String &p_request) override { GodotAlerts::get_singleton()->cancel(p_request); }
	void shutdown() override {
		if (GodotAlerts::get_singleton()) {
			GodotAlerts::get_singleton()->shutdown();
		}
	}
};
} //namespace
bool rn_register_alert_module(RNNativeModuleRegistry &p_registry, RNError &r_error) {
	RNModuleDefinition definition;
	definition.name = "GodotAlert";
	definition.factory = [] { return std::make_unique<RNAlertModule>(); };
	for (const char *name : { "getOrigin", "reply", "request" }) {
		RNMethodSchema method;
		method.name = name;
		method.result = RNValueSchema::value(String(name) == "getOrigin" || String(name) == "request" ? RNValueType::DYNAMIC : RNValueType::VOID);
		method.mode = String(name) == "request" ? RNCallMode::ASYNC : RNCallMode::SYNC;
		const int count = String(name) == "getOrigin" ? 1 : String(name) == "reply" ? 3
																					: 4;
		for (int i = 0; i < count; ++i) {
			RNArgumentSchema arg;
			arg.name = "argument" + String::num_int64(i);
			arg.value = RNValueSchema::value(RNValueType::DYNAMIC);
			method.arguments.push_back(arg);
		}
		definition.methods.push_back(method);
	}
	RNEventSchema event;
	event.name = "present";
	event.subscription_name = "onPresent";
	event.payload = RNValueSchema::value(RNValueType::DYNAMIC);
	definition.events.push_back(event);
	return p_registry.register_module(definition, r_error);
}
