#include "base_node.h"

#include "../singletons/react_native_file_singleton.h"

BaseNode::BaseNode() {
	label = memnew(Label);
	label->set_text("Bom dia");
	add_child(label);

	ReactNativeFileSingleton *file_singleton = ReactNativeFileSingleton::get_singleton();
	if (file_singleton) {
		if (!file_singleton->is_connected("react_native_file_changed", callable_mp(this, &BaseNode::_on_watched_file_changed))) {
			file_singleton->connect("react_native_file_changed", callable_mp(this, &BaseNode::_on_watched_file_changed));
		}

		if (file_singleton->has_file()) {
			set_label_text(file_singleton->get_file_content());
		}
	}

	hermes_runtime_singleton = HermesRuntimeSingleton::get_singleton();
}

BaseNode::~BaseNode() {
	ReactNativeFileSingleton *file_singleton = ReactNativeFileSingleton::get_singleton();
	if (file_singleton && file_singleton->is_connected("react_native_file_changed", callable_mp(this, &BaseNode::_on_watched_file_changed))) {
		file_singleton->disconnect("react_native_file_changed", callable_mp(this, &BaseNode::_on_watched_file_changed));
	}
}

void BaseNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_label_text", "text"), &BaseNode::set_label_text);
	ClassDB::bind_method(D_METHOD("get_label_text"), &BaseNode::get_label_text);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "label_text"), "set_label_text", "get_label_text");
}

String BaseNode::get_label_text() const {
	return label->get_text();
}

void BaseNode::set_label_text(const String &p_text) {
	if (p_text.is_empty()) {
		label->set_text("Bom Dia");
		return;
	}
	Variant res = hermes_runtime_singleton->evaluate(p_text);
	if (res.get_type() != Variant::STRING) {
		res = p_text;
	}
	label->set_text(res);
}

void BaseNode::_on_watched_file_changed(const String &p_path, const String &p_content, bool p_exists) {
	(void)p_path;

	if (!p_exists) {
		return;
	}

	if (p_content.is_empty()) {
		return;
	}

	set_label_text(p_content);
}
