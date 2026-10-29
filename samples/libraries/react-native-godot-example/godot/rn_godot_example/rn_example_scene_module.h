#pragma once

#include "react_native_bindings/turbomodules/rn_host_module_context.h"

#include "scene/main/node.h"

#include <RNGodotExampleSpec/RNGodotExampleSpecJSI.h>

#include <functional>
#include <memory>
#include <vector>

class RNExampleCounter : public Node {
	GDCLASS(RNExampleCounter, Node);

	NodePath root_view_path;
	double value = 0;
	Vector2 position;
	std::vector<std::weak_ptr<std::function<void(const Dictionary &)>>> listeners;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	static RNExampleCounter *for_root(Object *p_root);

	void set_root_view_path(const NodePath &p_path) { root_view_path = p_path; }
	NodePath get_root_view_path() const { return root_view_path; }
	Dictionary read() const;
	Dictionary increment(double p_amount);
	Dictionary echo(const Dictionary &p_value) const { return p_value; }
	void add_listener(const std::weak_ptr<std::function<void(const Dictionary &)>> &p_listener);
};

namespace facebook::react {

// An example TurboModule that reads and changes the RNExampleCounter
// registered for the calling root.
class RNExampleSceneModule : public NativeExampleSceneCxxSpec<RNExampleSceneModule> {
	RNHostModuleContext context;
	std::shared_ptr<std::function<void(const Dictionary &)>> changed;

public:
	RNExampleSceneModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext context);
	jsi::Object getConstants(jsi::Runtime &rt);
	jsi::Object read(jsi::Runtime &rt, double rootTag);
	jsi::Value incrementLater(jsi::Runtime &rt, double rootTag, double amount);
};

} // namespace facebook::react
