#pragma once

#include "scene/gui/control.h"
#include "scene/gui/label.h"

class BaseNode : public Control {
    GDCLASS(BaseNode, Control);

    String label_text;
    Label * label;

protected:
    static void _bind_methods();

    void _on_watched_file_changed(const String &p_path, const String &p_content, bool p_exists);

public:
    BaseNode();
    ~BaseNode() override;

    String get_label_text() const;
    void set_label_text(const String &p_text);
};
