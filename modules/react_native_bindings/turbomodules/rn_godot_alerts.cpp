#include "rn_godot_alerts.h"

#include "../root_view/react_native_root_view.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/gui/dialogs.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

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

void GodotAlerts::add_presenter(RNAlertPresenter p_presenter) {
	std::lock_guard lock(presenter_mutex);
	presenters.push_back(std::move(p_presenter));
}

void GodotAlerts::clear_presenters() {
	std::lock_guard lock(presenter_mutex);
	presenters.clear();
}

void GodotAlerts::_discard(const String &p_request, const RNError &p_error) {
	Request *pending = requests.getptr(p_request);
	if (!pending) {
		return;
	}
	const Request request = *pending;
	requests.erase(p_request);
	if (Node *dialog = Object::cast_to<Node>(ObjectDB::get_instance(request.dialog))) {
		dialog->queue_free();
	}
	request.completion(Dictionary(), p_error);
}

void GodotAlerts::_bubble(const String &p_request) {
	Request &pending = requests[p_request];
	pending.bubbled = true;
	ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(pending.root));
	Callable selected = root ? root->get_alert_handler() : Callable();
	pending.owner = pending.root;
	if (!selected.is_valid()) {
		selected = handler;
		pending.owner = get_instance_id();
	}
	if (!selected.is_valid()) {
		SceneTree *tree = SceneTree::get_singleton();
		_present_dialog(p_request, root ? static_cast<Node *>(root) : static_cast<Node *>(tree ? tree->get_root() : nullptr));
		return;
	}
	Variant origin;
	if (pending.root_tag) {
		Dictionary value;
		value["rootTag"] = pending.root_tag;
		origin = value;
	}
	selected.call_deferred(p_request, origin, pending.payload.duplicate(true));
}

void GodotAlerts::_present_dialog(const String &p_request, Node *p_parent) {
	Request &pending = requests[p_request];
	if (!p_parent || !p_parent->is_inside_tree()) {
		_discard(p_request, RNError::make(RNErrorCode::CANCELLED, "alert has no scene to present in", "GodotAlerts.request"));
		return;
	}
	pending.owner = get_instance_id();
	const Array buttons = pending.payload["buttons"];
	AcceptDialog *dialog = memnew(AcceptDialog);
	dialog->set_title(pending.payload["title"]);
	dialog->set_text(pending.payload["message"]);
	dialog->set_close_on_escape(bool(pending.payload["cancelable"]));
	dialog->get_ok_button()->set_text(Dictionary(buttons[0])["text"]);
	dialog->connect("confirmed", callable_mp(this, &GodotAlerts::_selected).bind(p_request, 0));
	for (int i = 1; i < buttons.size(); ++i) {
		Button *button = dialog->add_button(Dictionary(buttons[i])["text"], false, String::num_int64(i));
		button->connect("pressed", callable_mp(this, &GodotAlerts::_selected).bind(p_request, i));
	}
	dialog->connect("canceled", callable_mp(this, &GodotAlerts::_dismissed).bind(p_request));
	pending.dialog = dialog->get_instance_id();
	p_parent->add_child(dialog);
	dialog->popup_centered();
}

void GodotAlerts::request(Object *p_root, int p_root_tag, const Dictionary &p_payload, const String &p_mode, RNAlertCompletion p_completion) {
	const Array buttons = p_payload.get("buttons", Array());
	if (requests.size() >= 128 || buttons.is_empty() || buttons.size() > 64) {
		p_completion(Dictionary(), RNError::make(RNErrorCode::VALIDATION, "alert needs 1-64 buttons and at most 128 pending requests", "GodotAlerts.request"));
		return;
	}
	if (p_mode != "bubble" && p_mode != "native" && p_mode != "custom") {
		p_completion(Dictionary(), RNError::make(RNErrorCode::VALIDATION, "unknown alert mode", "GodotAlerts.request"));
		return;
	}
	if (p_root_tag && !p_root) {
		p_completion(Dictionary(), RNError::make(RNErrorCode::CANCELLED, "alert root is no longer running", "GodotAlerts.request"));
		return;
	}
	const String id = vformat("alert-%d", next_request++);
	Request pending;
	pending.completion = std::move(p_completion);
	pending.root = p_root ? p_root->get_instance_id() : ObjectID();
	pending.root_tag = p_root_tag;
	pending.payload = p_payload.duplicate(true);
	requests[id] = pending;
	if (p_mode == "native") {
		_present_dialog(id, Object::cast_to<Node>(p_root));
	} else if (p_mode == "custom") {
		bool presented = false;
		{
			std::lock_guard lock(presenter_mutex);
			std::erase_if(presenters, [&](const RNAlertPresenter &p_present) {
				const bool alive = p_present(id, p_root_tag, pending.payload);
				presented = presented || alive;
				return !alive;
			});
		}
		if (!presented) {
			_bubble(id);
		}
	} else {
		_bubble(id);
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
	const int64_t button_count = Array(pending->payload["buttons"]).size();
	if ((dismissed && index.get_type() != Variant::NIL) || (!dismissed && (index.get_type() != Variant::INT || int64_t(index) < 0 || int64_t(index) >= button_count))) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "invalid alert button result", "GodotAlerts.complete");
		return false;
	}
	const Request settled = *pending;
	requests.erase(p_request);
	if (Node *dialog = Object::cast_to<Node>(ObjectDB::get_instance(settled.dialog))) {
		dialog->queue_free();
	}
	settled.completion(p_result, RNError());
	return true;
}

bool GodotAlerts::complete_alert(const String &p_request, const Dictionary &p_result) {
	RNError error;
	return complete_for(get_instance_id(), p_request, p_result, error);
}

bool GodotAlerts::reply(int p_root_tag, const String &p_request, const Dictionary &p_result, RNError &r_error) {
	Request *pending = requests.getptr(p_request);
	if (!pending || pending->root_tag != p_root_tag || pending->bubbled || !pending->owner.is_null()) {
		r_error = RNError::make(RNErrorCode::CANCELLED, "custom presenter request is stale", "GodotAlerts.reply");
		return false;
	}
	pending->owner = pending->root;
	return complete_for(pending->root, p_request, p_result, r_error);
}

bool GodotAlerts::decline(int p_root_tag, const String &p_request, RNError &r_error) {
	Request *pending = requests.getptr(p_request);
	if (!pending || pending->root_tag != p_root_tag || pending->bubbled) {
		r_error = RNError::make(RNErrorCode::CANCELLED, "alert has already bubbled or is closed", "GodotAlerts.decline");
		return false;
	}
	_bubble(p_request);
	return true;
}

void GodotAlerts::_selected(const String &p_request, int p_button) {
	Dictionary result;
	result["buttonId"] = p_button;
	result["dismissed"] = false;
	RNError error;
	complete_for(get_instance_id(), p_request, result, error);
}

void GodotAlerts::_dismissed(const String &p_request) {
	Request *pending = requests.getptr(p_request);
	if (!pending) {
		return;
	}
	if (!bool(pending->payload["cancelable"])) {
		callable_mp(this, &GodotAlerts::_restore_dialog).call_deferred(p_request);
		return;
	}
	Dictionary result;
	result["buttonId"] = Variant();
	result["dismissed"] = true;
	RNError error;
	complete_for(get_instance_id(), p_request, result, error);
}

void GodotAlerts::_restore_dialog(const String &p_request) {
	Request *pending = requests.getptr(p_request);
	AcceptDialog *dialog = pending ? Object::cast_to<AcceptDialog>(ObjectDB::get_instance(pending->dialog)) : nullptr;
	if (!dialog || !dialog->is_inside_tree() || dialog->is_queued_for_deletion()) {
		_discard(p_request, RNError::make(RNErrorCode::CANCELLED, "alert was canceled", "GodotAlerts"));
		return;
	}
	dialog->popup_centered();
}

void GodotAlerts::close_surface(int p_root_tag) {
	Vector<String> closed;
	for (const KeyValue<String, Request> &entry : requests) {
		if (entry.value.root_tag == p_root_tag) {
			closed.push_back(entry.key);
		}
	}
	for (const String &id : closed) {
		_discard(id, RNError::make(RNErrorCode::CANCELLED, "alert root stopped", "GodotAlerts"));
	}
}

void GodotAlerts::shutdown() {
	Vector<String> closed;
	for (const KeyValue<String, Request> &entry : requests) {
		closed.push_back(entry.key);
	}
	for (const String &id : closed) {
		_discard(id, RNError::make(RNErrorCode::CANCELLED, "alert was canceled", "GodotAlerts"));
	}
}
