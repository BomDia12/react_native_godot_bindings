#include "rn_event_target.h"

#include <jsi/jsi.h>

#include <optional>

class RNEventTarget::Impl {
public:
	std::optional<facebook::jsi::WeakObject> instance_handle;
};

RNEventTarget::RNEventTarget(int p_tag, uint64_t p_generation) :
		tag(p_tag), generation(p_generation), impl(std::make_unique<Impl>()) {
}

RNEventTarget::RNEventTarget(int p_tag, uint64_t p_generation, facebook::jsi::Runtime &p_runtime, const facebook::jsi::Object &p_instance_handle) :
		tag(p_tag),
		generation(p_generation),
		impl(std::make_unique<Impl>()) {
	impl->instance_handle.emplace(p_runtime, p_instance_handle);
}

RNEventTarget::~RNEventTarget() = default;

bool RNEventTarget::has_instance_handle() const {
	return impl->instance_handle.has_value();
}

facebook::jsi::Value RNEventTarget::lock(facebook::jsi::Runtime &p_runtime) const {
	if (!impl->instance_handle.has_value()) {
		return facebook::jsi::Value::undefined();
	}
	return impl->instance_handle->lock(p_runtime);
}

void RNEventTarget::reset() {
	impl->instance_handle.reset();
}
