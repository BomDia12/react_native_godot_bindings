#include "rn_example_scene_module.h"

#include "../host/rn_main_thread_queue.h"
#include "../interop/rn_error.h"
#include "../turbomodules/rn_js_promise.h"

#include "core/object/class_db.h"

#include <jsi/JSIDynamic.h>

namespace {

HashMap<ObjectID, ObjectID> counters_by_root;

folly::dynamic counter_value(const Dictionary &p_value) {
	const Vector2 position = p_value["position"];
	return folly::dynamic::object("value", double(p_value["value"]))("x", position.x)("y", position.y);
}

} // namespace

void RNExampleCounter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_root_view_path", "path"), &RNExampleCounter::set_root_view_path);
	ClassDB::bind_method(D_METHOD("get_root_view_path"), &RNExampleCounter::get_root_view_path);
	ClassDB::bind_method(D_METHOD("read"), &RNExampleCounter::read);
	ClassDB::bind_method(D_METHOD("echo", "value"), &RNExampleCounter::echo);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "root_view_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "ReactNativeRootView"), "set_root_view_path", "get_root_view_path");
	ADD_SIGNAL(MethodInfo("changed", PropertyInfo(Variant::DICTIONARY, "value")));
}

void RNExampleCounter::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		if (Node *root = get_node_or_null(root_view_path)) {
			counters_by_root[root->get_instance_id()] = get_instance_id();
		}
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		Vector<ObjectID> roots;
		for (const KeyValue<ObjectID, ObjectID> &entry : counters_by_root) {
			if (entry.value == get_instance_id()) {
				roots.push_back(entry.key);
			}
		}
		for (ObjectID root : roots) {
			counters_by_root.erase(root);
		}
	}
}

RNExampleCounter *RNExampleCounter::for_root(Object *p_root) {
	const ObjectID *counter = p_root ? counters_by_root.getptr(p_root->get_instance_id()) : nullptr;
	return counter ? Object::cast_to<RNExampleCounter>(ObjectDB::get_instance(*counter)) : nullptr;
}

Dictionary RNExampleCounter::read() const {
	Dictionary result;
	result["value"] = value;
	result["position"] = position;
	return result;
}

Dictionary RNExampleCounter::increment(double p_amount) {
	value += p_amount;
	position.x += p_amount;
	const Dictionary result = read();
	emit_signal("changed", result);
	for (const auto &listener : listeners) {
		if (auto callback = listener.lock()) {
			(*callback)(result);
		}
	}
	return result;
}

void RNExampleCounter::add_listener(const std::weak_ptr<std::function<void(const Dictionary &)>> &p_listener) {
	for (const auto &listener : listeners) {
		if (!listener.owner_before(p_listener) && !p_listener.owner_before(listener)) {
			return;
		}
	}
	listeners.push_back(p_listener);
}

namespace facebook::react {

RNExampleSceneModule::RNExampleSceneModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_context) :
		NativeExampleSceneCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
	changed = std::make_shared<std::function<void(const Dictionary &)>>([this](const Dictionary &p_value) {
		static_cast<AsyncEventEmitter<jsi::Value> &>(*eventEmitterMap_["onChanged"]).emit([value = counter_value(p_value)](jsi::Runtime &p_runtime) { return jsi::valueFromDynamic(p_runtime, value); });
	});
}

jsi::Object RNExampleSceneModule::getConstants(jsi::Runtime &rt) {
	jsi::Object constants(rt);
	constants.setProperty(rt, "name", "ExampleScene");
	constants.setProperty(rt, "version", 1);
	return constants;
}

jsi::Object RNExampleSceneModule::read(jsi::Runtime &rt, double rootTag) {
	Dictionary value;
	bool found = false;
	context.main_thread->run_sync([&] {
		RNExampleCounter *counter = RNExampleCounter::for_root(context.find_root(int(rootTag)));
		if (counter) {
			counter->add_listener(changed);
			value = counter->read();
			found = true;
		}
	});
	if (!found) {
		rn_throw_js_error(rt, RNErrorCode::OBJECT_GONE, "this root has no registered RNExampleCounter");
	}
	return jsi::valueFromDynamic(rt, counter_value(value)).asObject(rt);
}

jsi::Value RNExampleSceneModule::incrementLater(jsi::Runtime &rt, double rootTag, double amount) {
	auto main_thread = context.main_thread;
	auto find_root = context.find_root;
	return RNJSPromise::create(rt, jsInvoker_, [=](std::shared_ptr<RNJSPromise> p_promise) {
		main_thread->post([=] {
			RNExampleCounter *counter = RNExampleCounter::for_root(find_root(int(rootTag)));
			if (!counter) {
				p_promise->reject(RNErrorCode::OBJECT_GONE, "this root has no registered RNExampleCounter");
				return;
			}
			const folly::dynamic result = counter_value(counter->increment(amount));
			p_promise->resolve([result](jsi::Runtime &p_runtime) { return jsi::valueFromDynamic(p_runtime, result); });
		});
	});
}

} // namespace facebook::react
