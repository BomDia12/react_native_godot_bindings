#pragma once

#include "rn_native_module_registry.h"

#include "scene/main/node.h"

struct RNRuntimeCoordinatorState;
class RNApplicationLifecycle : public Node {
	GDCLASS(RNApplicationLifecycle, Node);

	std::weak_ptr<RNRuntimeCoordinatorState> state;

protected:
	static void _bind_methods() {}
	void _notification(int p_notification);

public:
	void configure(const std::shared_ptr<RNRuntimeCoordinatorState> &p_state) { state = p_state; }
};

bool rn_register_application_services(RNNativeModuleRegistry &p_registry, const std::shared_ptr<RNRuntimeCoordinatorState> &p_state, RNError &r_error);
