#pragma once

#include "../interop/rn_error.h"

#include "core/input/input_event.h"
#include "core/variant/dictionary.h"

// The JSON-compatible form of an InputMap event, which JavaScript shows, rebinds
// with and saves.
Dictionary rn_input_event_to_value(const Ref<InputEvent> &p_event);
Ref<InputEvent> rn_input_event_from_value(const Dictionary &p_value, RNError &r_error);
