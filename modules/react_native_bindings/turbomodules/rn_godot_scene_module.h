#pragma once

#include "../interop/rn_object_registry.h"
#include "../interop/rn_scene_binding.h"

#include <ReactNativeGodotSpec/ReactNativeGodotSpecJSI.h>

#include <functional>
#include <map>
#include <mutex>

class RNMainThreadQueue;
class ReactNativeRootView;

using RNRootLookup = std::function<ReactNativeRootView *(int p_root_tag)>;

// Main-thread state behind the GodotScene module: one binding per running root,
// connected to the signals its scene binding declares.
class RNSceneBindings {
	struct Connection {
		ObjectID target;
		StringName signal;
		Callable callback;
	};
	struct Binding {
		std::shared_ptr<const RNSceneAttachment> attachment;
		String handle;
		Vector<Connection> connections;
	};

	RNRootLookup find_root;
	RNObjectRegistry objects;
	std::map<int, Binding> bindings;
	uint64_t sequence = 0;
	std::mutex emit_mutex;
	std::vector<std::function<bool(const Variant &)>> emitters;

	void _disconnect(Binding &p_binding);
	void _publish(int p_root_tag, const Binding &p_binding, const StringName &p_event, const Variant &p_payload);
	bool _refresh(int p_root_tag, Binding &p_binding, RNError &r_error);
	Binding *_binding(int p_root_tag, RNError &r_error);
	Dictionary _envelope(const Binding &p_binding) const;

public:
	explicit RNSceneBindings(RNRootLookup p_find_root) :
			find_root(std::move(p_find_root)) {}
	~RNSceneBindings() { shutdown(); }

	// Each module instance listens until its emitter reports that it is gone.
	void add_emitter(std::function<bool(const Variant &)> p_emit);
	bool get_binding(int p_root_tag, Dictionary &r_binding, RNError &r_error);
	bool invoke(int p_root_tag, const String &p_handle, const StringName &p_command, const Array &p_arguments, bool p_async, Variant &r_result, RNError &r_error);
	void binding_changed(int p_root_tag);
	void close_surface(int p_root_tag);
	void process_frame();
	void shutdown();
};

namespace facebook::react {

struct RNSceneModuleContext {
	std::shared_ptr<RNSceneBindings> scenes;
	std::shared_ptr<RNMainThreadQueue> main_thread;
};

class RNGodotSceneModule : public NativeGodotSceneCxxSpec<RNGodotSceneModule> {
	RNSceneModuleContext context;
	jsi::Value _invoke(jsi::Runtime &rt, int p_root_tag, const std::string &p_handle, const std::string &p_command, const jsi::Value &p_arguments, bool p_async);

public:
	RNGodotSceneModule(std::shared_ptr<CallInvoker> jsInvoker, RNSceneModuleContext context);
	jsi::Object getBinding(jsi::Runtime &rt, double rootTag);
	jsi::Object read(jsi::Runtime &rt, double rootTag, const std::string &binding);
	jsi::Value call(jsi::Runtime &rt, double rootTag, const std::string &binding, const std::string &command, jsi::Array args);
	jsi::Value callAsync(jsi::Runtime &rt, double rootTag, const std::string &binding, const std::string &command, jsi::Array args);
	void emit_changed(const Variant &p_change);
};

} // namespace facebook::react
