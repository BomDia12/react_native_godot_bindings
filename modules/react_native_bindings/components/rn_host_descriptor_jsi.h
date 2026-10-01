#pragma once

#include <jsi/jsi.h>

#include <memory>

class RNHostDescriptorRegistry;

class RNHostDescriptorJSIRegistry : public facebook::jsi::HostObject {
	std::shared_ptr<RNHostDescriptorRegistry> registry;

public:
	explicit RNHostDescriptorJSIRegistry(const std::shared_ptr<RNHostDescriptorRegistry> &p_registry) :
			registry(p_registry) {}

	facebook::jsi::Value get(facebook::jsi::Runtime &p_runtime, const facebook::jsi::PropNameID &p_name) override;
	std::vector<facebook::jsi::PropNameID> getPropertyNames(facebook::jsi::Runtime &p_runtime) override;
};
