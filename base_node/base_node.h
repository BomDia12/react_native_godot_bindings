#pragma once

#include "scene/gui/control.h"
#include "scene/gui/label.h"

class BaseNode : public Control {
    GDCLASS(BaseNode, Control);

    String label_text;
    Label * label;

protected:
    static void _bind_methods();

public:
    BaseNode();

    String get_label_text() const;
    void set_label_text(const String &p_text);
};
