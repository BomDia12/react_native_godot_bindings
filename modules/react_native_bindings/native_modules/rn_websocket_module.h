#pragma once

#include "../runtime/rn_service_settings.h"
#include "rn_blob_service.h"

bool rn_register_websocket_module(RNNativeModuleRegistry &p_registry, const std::function<RNServiceSettings()> &p_settings, const std::function<std::shared_ptr<RNBlobService>()> &p_blobs, RNError &r_error);
