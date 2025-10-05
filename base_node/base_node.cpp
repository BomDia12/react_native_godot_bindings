#include "base_node.h"

BaseNode::BaseNode() {
    label_text = "Bom Dia";

    label = memnew(Label);
    label->set_text(label_text);
    add_child(label);
}

void BaseNode::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_label_text", "text"), &BaseNode::set_label_text);
    ClassDB::bind_method(D_METHOD("get_label_text"), &BaseNode::get_label_text);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "label_text"), "set_label_text", "get_label_text");
}

String BaseNode::get_label_text() const {
    return label_text;
}

void BaseNode::set_label_text(const String &p_text) {
    label_text = p_text;
    label->set_text(label_text);
}
