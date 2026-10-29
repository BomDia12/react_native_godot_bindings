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
using RNModuleLookup = std::function<std::shared_ptr<const RNSceneAttachment>(const String &p_module)>;

// Main-thread state behind the GodotScene module: one binding per running root
// and per registered application module, connected to the signals its scene
// binding declares.
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
	RNModuleLookup find_module;
	RNObjectRegistry objects;
	std::map<RNBindingScope, Binding> bindings;
	uint64_t sequence = 0;
	std::mutex emit_mutex;
	std::vector<std::function<bool(const Variant &)>> emitters;

	void _disconnect(Binding &p_binding);
	void _publish(const RNBindingScope &p_scope, const Binding &p_binding, const StringName &p_event, const Variant &p_payload);
	std::shared_ptr<const RNSceneAttachment> _attachment(const RNBindingScope &p_scope) const;
	bool _refresh(const RNBindingScope &p_scope, Binding &p_binding, RNError &r_error);
	Binding *_binding(const RNBindingScope &p_scope, RNError &r_error);
	Dictionary _envelope(const Binding &p_binding) const;

public:
	explicit RNSceneBindings(RNRootLookup p_find_root, RNModuleLookup p_find_module = nullptr) :
			find_root(std::move(p_find_root)), find_module(std::move(p_find_module)) {}
	~RNSceneBindings() { shutdown(); }

	// Each module instance listens until its emitter reports that it is gone.
	void add_emitter(std::function<bool(const Variant &)> p_emit);
	bool get_binding(const RNBindingScope &p_scope, Dictionary &r_binding, RNError &r_error);
	bool invoke(const RNBindingScope &p_scope, const String &p_handle, const StringName &p_command, const Array &p_arguments, bool p_async, Variant &r_result, RNError &r_error);
	void binding_changed(const RNBindingScope &p_scope);
	void close_scope(const RNBindingScope &p_scope);
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
	jsi::Object _get_binding(jsi::Runtime &rt, const RNBindingScope &p_scope);
	jsi::Value _invoke(jsi::Runtime &rt, const RNBindingScope &p_scope, const std::string &p_handle, const std::string &p_command, const jsi::Value &p_arguments, bool p_async);
	jsi::Value _invoke_async(jsi::Runtime &rt, const RNBindingScope &p_scope, const std::string &p_handle, const std::string &p_command, jsi::Array p_arguments);

public:
	RNGodotSceneModule(std::shared_ptr<CallInvoker> jsInvoker, RNSceneModuleContext context);
	jsi::Object getBinding(jsi::Runtime &rt, double rootTag);
	jsi::Object read(jsi::Runtime &rt, double rootTag, const std::string &binding);
	jsi::Value call(jsi::Runtime &rt, double rootTag, const std::string &binding, const std::string &command, jsi::Array args);
	jsi::Value callAsync(jsi::Runtime &rt, double rootTag, const std::string &binding, const std::string &command, jsi::Array args);
	jsi::Object getModule(jsi::Runtime &rt, const std::string &name);
	jsi::Object readModule(jsi::Runtime &rt, const std::string &name, const std::string &binding);
	jsi::Value callModule(jsi::Runtime &rt, const std::string &name, const std::string &binding, const std::string &command, jsi::Array args);
	jsi::Value callModuleAsync(jsi::Runtime &rt, const std::string &name, const std::string &binding, const std::string &command, jsi::Array args);
	void emit_changed(const Variant &p_change);
};

} // namespace facebook::react
