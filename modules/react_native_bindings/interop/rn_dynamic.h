#pragma once

#include "core/variant/variant.h"

#include <folly/dynamic.h>

Variant rn_variant_from_dynamic(const folly::dynamic &p_value);
folly::dynamic rn_dynamic_from_variant(const Variant &p_value);
