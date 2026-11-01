#include "rn_game_modules.h"

#include "../components/rn_resource_requests.h"
#include "../host/rn_game_services.h"
#include "../host/rn_main_thread_queue.h"
#include "../input/rn_input_event_value.h"
#include "../interop/rn_error.h"
#include "../interop/rn_value_codec.h"
#include "rn_js_promise.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/input/input_map.h"
#include "core/string/translation_server.h"
#include "servers/audio/audio_server.h"
#include "servers/audio/audio_stream.h"

namespace facebook::react {

namespace {

constexpr const char *SHUTTING_DOWN = "React Native is shutting down";

String to_godot(jsi::Runtime &rt, const jsi::String &p_string) {
	return String::utf8(p_string.utf8(rt).c_str());
}

jsi::String to_js(jsi::Runtime &rt, const String &p_string) {
	return jsi::String::createFromUtf8(rt, p_string.utf8().get_data());
}

Variant value_from_js(jsi::Runtime &rt, const jsi::Value &p_value, const char *p_operation) {
	RNDecodedValue decoded = RNValueCodec::from_js(rt, p_value, RNValueSchema::value(RNValueType::DYNAMIC), p_operation, "value");
	if (!decoded.ok()) {
		rn_throw_js_error(rt, decoded.error.code.utf8().get_data(), decoded.error.message.utf8().get_data());
	}
	return decoded.value;
}

jsi::Value value_to_js(jsi::Runtime &rt, const Variant &p_value, const char *p_operation) {
	jsi::Value value;
	RNError error;
	if (!RNValueCodec::to_js(rt, p_value, RNValueSchema::value(RNValueType::DYNAMIC), value, error, nullptr, p_operation, "value")) {
		rn_throw_js_error(rt, error.code.utf8().get_data(), error.message.utf8().get_data());
	}
	return value;
}

// Runs p_work on the main thread and throws its failure into JavaScript. Hooks
// read through here while rendering, and a runtime going away can still render,
// so its calls do nothing and the callers return their empty values.
void run_on_main(jsi::Runtime &rt, RNMainThreadQueue &p_queue, const std::function<void(RNError &)> &p_work) {
	RNError error;
	p_queue.run_sync([&] { p_work(error); });
	if (error.is_set()) {
		rn_throw_js_error(rt, error.code.utf8().get_data(), error.message.utf8().get_data());
	}
}

Array joypads_value() {
	Array joypads;
	const TypedArray<int> devices = Input::get_singleton()->get_connected_joypads();
	for (int i = 0; i < devices.size(); ++i) {
		Dictionary joypad;
		joypad["device"] = devices[i];
		joypad["name"] = Input::get_singleton()->get_joy_name(devices[i]);
		joypads.push_back(joypad);
	}
	return joypads;
}

Array buses_value() {
	AudioServer *audio = AudioServer::get_singleton();
	Array buses;
	for (int i = 0; i < audio->get_bus_count(); ++i) {
		Dictionary bus;
		bus["name"] = audio->get_bus_name(i);
		bus["volume"] = audio->get_bus_volume_linear(i);
		bus["muted"] = audio->is_bus_mute(i);
		buses.push_back(bus);
	}
	return buses;
}

int bus_index(const String &p_name, RNError &r_error) {
	const int index = AudioServer::get_singleton()->get_bus_index(p_name);
	if (index < 0) {
		r_error = RNError::make(RNErrorCode::VALIDATION, vformat("'%s' is not an audio bus.", p_name));
	}
	return index;
}

} // namespace

RNGodotInputModule::RNGodotInputModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_host, std::shared_ptr<RNGameServicesHandle> p_services) :
		NativeGodotInputCxxSpec(std::move(jsInvoker)), host(std::move(p_host)), services(std::move(p_services)) {
}

std::shared_ptr<RNGodotInputModule> RNGodotInputModule::create(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_host, std::shared_ptr<RNGameServicesHandle> p_services) {
	std::shared_ptr<RNGodotInputModule> module(new RNGodotInputModule(std::move(jsInvoker), std::move(p_host), std::move(p_services)));
	module->_connect();
	return module;
}

void RNGodotInputModule::_connect() {
	host.main_thread->post([weak = weak_from_this(), services = services] {
		RNGameServices *node = services->get();
		if (!node) {
			return;
		}
		node->set_input_callbacks(
				[weak](const String &p_action, bool p_pressed, float p_strength, bool p_echo) {
					auto alive = weak.lock();
					if (!alive) {
						return;
					}
					Dictionary event;
					event["action"] = p_action;
					event["pressed"] = p_pressed;
					event["strength"] = p_strength;
					event["echo"] = p_echo;
					static_cast<AsyncEventEmitter<jsi::Value> &>(*alive->eventEmitterMap_["onAction"]).emit([event](jsi::Runtime &rt) -> jsi::Value { return value_to_js(rt, event, "GodotInput.onAction"); });
				},
				[weak](const String &p_device) {
					if (auto alive = weak.lock()) {
						alive->emitOnLastDeviceChanged(std::string(p_device.utf8().get_data()));
					}
				},
				[weak] {
					if (auto alive = weak.lock()) {
						alive->_emit_joypads();
					}
				});
	});
}

void RNGodotInputModule::_emit_joypads() {
	const Array joypads = joypads_value();
	static_cast<AsyncEventEmitter<jsi::Value> &>(*eventEmitterMap_["onJoypadsChanged"]).emit([joypads](jsi::Runtime &rt) -> jsi::Value { return value_to_js(rt, joypads, "GodotInput.onJoypadsChanged"); });
}

void RNGodotInputModule::_require_action(jsi::Runtime &rt, const String &p_action) {
	run_on_main(rt, *host.main_thread, [&](RNError &r_error) {
		if (!InputMap::get_singleton()->has_action(p_action)) {
			r_error = RNError::make(RNErrorCode::VALIDATION, vformat("'%s' is not an input action.", p_action));
		}
	});
}

void RNGodotInputModule::watchAction(jsi::Runtime &rt, jsi::String name) {
	const String action = to_godot(rt, name);
	_require_action(rt, action);
	host.main_thread->post([services = services, action] {
		if (RNGameServices *node = services->get()) {
			node->watch_action(action);
		}
	});
}

void RNGodotInputModule::unwatchAction(jsi::Runtime &rt, jsi::String name) {
	host.main_thread->post([services = services, action = to_godot(rt, name)] {
		if (RNGameServices *node = services->get()) {
			node->unwatch_action(action);
		}
	});
}

jsi::Value RNGodotInputModule::getActions(jsi::Runtime &rt) {
	Array actions;
	run_on_main(rt, *host.main_thread, [&](RNError &) {
		for (const Variant &action : InputMap::get_singleton()->get_actions()) {
			actions.push_back(String(action));
		}
	});
	return value_to_js(rt, actions, "GodotInput.getActions");
}

jsi::Value RNGodotInputModule::getActionEvents(jsi::Runtime &rt, jsi::String name) {
	const String action = to_godot(rt, name);
	_require_action(rt, action);
	Array events;
	run_on_main(rt, *host.main_thread, [&](RNError &) {
		for (const Ref<InputEvent> &event : *InputMap::get_singleton()->action_get_events(action)) {
			const Dictionary value = rn_input_event_to_value(event);
			if (!value.is_empty()) {
				events.push_back(value);
			}
		}
	});
	return value_to_js(rt, events, "GodotInput.getActionEvents");
}

void RNGodotInputModule::setActionEvents(jsi::Runtime &rt, jsi::String name, jsi::Value events) {
	const String action = to_godot(rt, name);
	const Variant values = value_from_js(rt, events, "GodotInput.setActionEvents");
	_require_action(rt, action);
	run_on_main(rt, *host.main_thread, [&](RNError &r_error) {
		if (values.get_type() != Variant::ARRAY) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "Input events must be an array.");
			return;
		}
		const Array list = values;
		Vector<Ref<InputEvent>> parsed;
		for (int i = 0; i < list.size(); ++i) {
			parsed.push_back(rn_input_event_from_value(list[i], r_error));
			if (r_error.is_set()) {
				return;
			}
		}
		InputMap *map = InputMap::get_singleton();
		map->action_erase_events(action);
		for (const Ref<InputEvent> &event : parsed) {
			map->action_add_event(action, event);
		}
	});
}

void RNGodotInputModule::resetAction(jsi::Runtime &rt, jsi::String name) {
	const String action = to_godot(rt, name);
	_require_action(rt, action);
	run_on_main(rt, *host.main_thread, [&](RNError &) {
		InputMap *map = InputMap::get_singleton();
		map->action_erase_events(action);
		const String setting = "input/" + action;
		ProjectSettings *settings = ProjectSettings::get_singleton();
		if (settings->has_setting(setting)) {
			const Array defaults = Dictionary(settings->get_setting(setting)).get("events", Array());
			for (const Ref<InputEvent> event : defaults) {
				map->action_add_event(action, event);
			}
		} else if (const List<Ref<InputEvent>> *builtin = map->get_builtins().getptr(action)) {
			for (const Ref<InputEvent> &event : *builtin) {
				map->action_add_event(action, event);
			}
		}
	});
}

jsi::Value RNGodotInputModule::captureNextEvent(jsi::Runtime &rt, jsi::Value devices) {
	const Variant requested = value_from_js(rt, devices, "GodotInput.captureNextEvent");
	return RNJSPromise::create(rt, jsInvoker_, [this, requested](std::shared_ptr<RNJSPromise> p_promise) {
		PackedStringArray filter;
		for (const Variant &device : Array(requested)) {
			filter.push_back(device);
		}
		host.main_thread->post([services = services, filter, p_promise] {
			RNGameServices *node = services->get();
			if (!node) {
				p_promise->reject(RNErrorCode::CANCELLED, SHUTTING_DOWN);
				return;
			}
			node->capture_next_event(filter, [p_promise](const Ref<InputEvent> &p_event) {
				if (p_event.is_null()) {
					p_promise->reject(RNErrorCode::CANCELLED, "The input capture was cancelled.");
					return;
				}
				const Dictionary value = rn_input_event_to_value(p_event);
				p_promise->resolve([value](jsi::Runtime &p_runtime) { return value_to_js(p_runtime, value, "GodotInput.captureNextEvent"); });
			});
		});
	});
}

void RNGodotInputModule::cancelCapture(jsi::Runtime &) {
	host.main_thread->post([services = services] {
		if (RNGameServices *node = services->get()) {
			node->cancel_capture();
		}
	});
}

jsi::String RNGodotInputModule::getLastDevice(jsi::Runtime &rt) {
	String device;
	run_on_main(rt, *host.main_thread, [&](RNError &) {
		if (RNGameServices *node = services->get()) {
			device = node->get_last_device();
		}
	});
	return to_js(rt, device);
}

jsi::Value RNGodotInputModule::getJoypads(jsi::Runtime &rt) {
	Array joypads;
	run_on_main(rt, *host.main_thread, [&](RNError &) { joypads = joypads_value(); });
	return value_to_js(rt, joypads, "GodotInput.getJoypads");
}

void RNGodotInputModule::vibrateJoypad(jsi::Runtime &, double device, double weak, double strong, double durationMs) {
	host.main_thread->post([device = int(device), weak, strong, durationMs] {
		Input::get_singleton()->start_joy_vibration(device, weak, strong, durationMs / 1000.0);
	});
}

void RNGodotInputModule::stopJoypadVibration(jsi::Runtime &, double device) {
	host.main_thread->post([device = int(device)] { Input::get_singleton()->stop_joy_vibration(device); });
}

RNGodotAudioModule::RNGodotAudioModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_host, std::shared_ptr<RNGameServicesHandle> p_services) :
		NativeGodotAudioCxxSpec(std::move(jsInvoker)), host(std::move(p_host)), services(std::move(p_services)) {
}

std::shared_ptr<RNGodotAudioModule> RNGodotAudioModule::create(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_host, std::shared_ptr<RNGameServicesHandle> p_services) {
	std::shared_ptr<RNGodotAudioModule> module(new RNGodotAudioModule(std::move(jsInvoker), std::move(p_host), std::move(p_services)));
	module->_connect();
	return module;
}

void RNGodotAudioModule::_connect() {
	host.main_thread->post([weak = weak_from_this(), services = services] {
		if (RNGameServices *node = services->get()) {
			node->set_buses_callback([weak] {
				if (auto alive = weak.lock()) {
					alive->_emit_buses();
				}
			});
		}
	});
}

void RNGodotAudioModule::_emit_buses() {
	const Array buses = buses_value();
	static_cast<AsyncEventEmitter<jsi::Value> &>(*eventEmitterMap_["onBusesChanged"]).emit([buses](jsi::Runtime &rt) -> jsi::Value { return value_to_js(rt, buses, "GodotAudio.onBusesChanged"); });
}

// Fire and forget: a click sound never holds JavaScript up, and a failure can
// only be logged.
void RNGodotAudioModule::play(jsi::Runtime &rt, jsi::String source, jsi::String bus, double volumeDb, double pitchScale) {
	host.main_thread->post([services = services, path = to_godot(rt, source), bus_name = to_godot(rt, bus), volumeDb, pitchScale] {
		RNResourceRequests::request(path, "AudioStream", [services, path, bus_name, volumeDb, pitchScale](const Ref<Resource> &p_resource, const String &p_error) {
			const Ref<AudioStream> stream = p_resource;
			if (stream.is_null()) {
				ERR_PRINT(vformat("E_RESOURCE: GodotAudio.play: %s", p_error.is_empty() ? vformat("'%s' is not an AudioStream.", path) : p_error));
				return;
			}
			if (AudioServer::get_singleton()->get_bus_index(bus_name) < 0) {
				ERR_PRINT(vformat("E_VALIDATION: GodotAudio.play: '%s' is not an audio bus.", bus_name));
				return;
			}
			if (RNGameServices *node = services->get()) {
				node->play_audio(path, stream, bus_name, volumeDb, pitchScale);
			}
		});
	});
}

jsi::Value RNGodotAudioModule::preload(jsi::Runtime &rt, jsi::Value sources) {
	const Variant requested = value_from_js(rt, sources, "GodotAudio.preload");
	return RNJSPromise::create(rt, jsInvoker_, [this, requested](std::shared_ptr<RNJSPromise> p_promise) {
		host.main_thread->post([requested, p_promise] {
			const Array paths = requested;
			if (paths.is_empty()) {
				p_promise->resolve([](jsi::Runtime &) { return jsi::Value::undefined(); });
				return;
			}
			auto remaining = std::make_shared<int>(paths.size());
			for (const Variant &path : paths) {
				RNResourceRequests::request(path, "AudioStream", [remaining, p_promise](const Ref<Resource> &p_resource, const String &p_error) {
					if (Object::cast_to<AudioStream>(*p_resource) == nullptr) {
						p_promise->reject(RNErrorCode::VALIDATION, p_error.is_empty() ? "A preloaded source is not an AudioStream." : p_error.utf8().get_data());
					} else if (--*remaining == 0) {
						p_promise->resolve([](jsi::Runtime &) { return jsi::Value::undefined(); });
					}
				});
			}
		});
	});
}

void RNGodotAudioModule::stopAll(jsi::Runtime &rt, std::optional<jsi::String> bus) {
	host.main_thread->post([services = services, bus_name = bus ? to_godot(rt, *bus) : String()] {
		if (RNGameServices *node = services->get()) {
			node->stop_audio(bus_name.is_empty() ? StringName() : StringName(bus_name));
		}
	});
}

jsi::Value RNGodotAudioModule::getBuses(jsi::Runtime &rt) {
	Array buses;
	run_on_main(rt, *host.main_thread, [&](RNError &) { buses = buses_value(); });
	return value_to_js(rt, buses, "GodotAudio.getBuses");
}

void RNGodotAudioModule::setBusVolume(jsi::Runtime &rt, jsi::String name, double linear) {
	const String bus = to_godot(rt, name);
	run_on_main(rt, *host.main_thread, [&](RNError &r_error) {
		const int index = bus_index(bus, r_error);
		if (index >= 0) {
			AudioServer::get_singleton()->set_bus_volume_linear(index, linear);
			if (RNGameServices *node = services->get()) {
				node->notify_buses_changed();
			}
		}
	});
}

void RNGodotAudioModule::setBusMute(jsi::Runtime &rt, jsi::String name, bool muted) {
	const String bus = to_godot(rt, name);
	run_on_main(rt, *host.main_thread, [&](RNError &r_error) {
		const int index = bus_index(bus, r_error);
		if (index >= 0) {
			AudioServer::get_singleton()->set_bus_mute(index, muted);
			if (RNGameServices *node = services->get()) {
				node->notify_buses_changed();
			}
		}
	});
}

RNGodotTranslationModule::RNGodotTranslationModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_host, std::shared_ptr<RNGameServicesHandle> p_services) :
		NativeGodotTranslationCxxSpec(std::move(jsInvoker)), host(std::move(p_host)), services(std::move(p_services)) {
}

std::shared_ptr<RNGodotTranslationModule> RNGodotTranslationModule::create(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_host, std::shared_ptr<RNGameServicesHandle> p_services) {
	std::shared_ptr<RNGodotTranslationModule> module(new RNGodotTranslationModule(std::move(jsInvoker), std::move(p_host), std::move(p_services)));
	module->_connect();
	return module;
}

void RNGodotTranslationModule::_connect() {
	host.main_thread->post([weak = weak_from_this(), services = services] {
		if (RNGameServices *node = services->get()) {
			node->set_translation_callback([weak] {
				if (auto alive = weak.lock()) {
					alive->emitOnLocaleChanged(std::string(TranslationServer::get_singleton()->get_locale().utf8().get_data()));
				}
			});
		}
	});
}

jsi::String RNGodotTranslationModule::translate(jsi::Runtime &rt, jsi::String message, jsi::String context) {
	const String source = to_godot(rt, message);
	const String source_context = to_godot(rt, context);
	String translated = source;
	run_on_main(rt, *host.main_thread, [&](RNError &) { translated = TranslationServer::get_singleton()->translate(source, source_context); });
	return to_js(rt, translated);
}

jsi::String RNGodotTranslationModule::translatePlural(jsi::Runtime &rt, jsi::String message, jsi::String plural, double n, jsi::String context) {
	const String source = to_godot(rt, message);
	const String source_plural = to_godot(rt, plural);
	const String source_context = to_godot(rt, context);
	String translated = source;
	run_on_main(rt, *host.main_thread, [&](RNError &) { translated = TranslationServer::get_singleton()->translate_plural(source, source_plural, int(n), source_context); });
	return to_js(rt, translated);
}

jsi::String RNGodotTranslationModule::getLocale(jsi::Runtime &rt) {
	String locale;
	run_on_main(rt, *host.main_thread, [&](RNError &) { locale = TranslationServer::get_singleton()->get_locale(); });
	return to_js(rt, locale);
}

void RNGodotTranslationModule::setLocale(jsi::Runtime &rt, jsi::String locale) {
	const String next = to_godot(rt, locale);
	run_on_main(rt, *host.main_thread, [&](RNError &) { TranslationServer::get_singleton()->set_locale(next); });
}

jsi::Array RNGodotTranslationModule::getLoadedLocales(jsi::Runtime &rt) {
	PackedStringArray locales;
	run_on_main(rt, *host.main_thread, [&](RNError &) { locales = TranslationServer::get_singleton()->get_loaded_locales(); });
	jsi::Array result(rt, locales.size());
	for (int i = 0; i < locales.size(); ++i) {
		result.setValueAtIndex(rt, i, to_js(rt, locales[i]));
	}
	return result;
}

} // namespace facebook::react
