#pragma once

#include "rn_error.h"

bool rn_normalize_local_resource_path(const String &p_path, String &r_normalized, RNError &r_error, const String &p_operation = "resourcePath");
