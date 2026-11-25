#pragma once

#include "core/variant/variant.h"

#include <folly/dynamic.h>

Variant rn_variant_from_dynamic(const folly::dynamic &p_value);
// As rn_variant_from_dynamic, but the Color and Vector wrappers of
// react-native-godot/values become their Godot types.
Variant rn_typed_variant_from_dynamic(const folly::dynamic &p_value);
folly::dynamic rn_dynamic_from_variant(const Variant &p_value);
// The path of the first value in p_value that rn_dynamic_from_variant can't carry
// exactly, or an empty String.
String rn_dynamic_unsupported_path(const Variant &p_value, const String &p_path);
// p_properties without the values rn_dynamic_from_variant can't carry exactly,
// each reported as an error.
Dictionary rn_plain_properties(const Dictionary &p_properties);
