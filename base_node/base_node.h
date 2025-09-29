#pragma once

#include "scene/gui/control.h"

class BaseNode : public Control {
    GDCLASS(BaseNode, Control);

    char * label_text = "Bom Dia";

protected:
    static void _bind_methods();

public:
    BaseNode();
    ~BaseNode();

};
