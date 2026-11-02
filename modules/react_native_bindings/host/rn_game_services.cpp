#include "rn_game_services.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/input/input_map.h"
#include "core/object/callable_mp.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/gui/rich_text_label.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/audio_stream_polyphonic.h"
#include "servers/audio/audio_server.h"

namespace {

constexpr float JOYPAD_AXIS_THRESHOLD = 0.5;

} // namespace

String rn_input_device_of(const Ref<InputEvent> &p_event) {
	// Godot's mouse emulation of touch and the reverse aren't a device the player used.
	if (p_event->get_device() == InputEvent::DEVICE_ID_EMULATION) {
		return String();
	}
	if (Object::cast_to<InputEventKey>(*p_event)) {
		return "keyboard";
	}
	if (Object::cast_to<InputEventMouse>(*p_event)) {
		return "mouse";
	}
	if (Object::cast_to<InputEventJoypadButton>(*p_event)) {
		return "joypad";
	}
	if (const InputEventJoypadMotion *motion = Object::cast_to<InputEventJoypadMotion>(*p_event)) {
		return Math::abs(motion->get_axis_value()) >= JOYPAD_AXIS_THRESHOLD ? "joypad" : "";
	}
	if (Object::cast_to<InputEventScreenTouch>(*p_event) || Object::cast_to<InputEventScreenDrag>(*p_event)) {
		return "touch";
	}
	return String();
}

RNGameServices::RNGameServices() {
	set_process_mode(PROCESS_MODE_ALWAYS);
	set_name("RNGameServices");
	set_process_input(true);
	set_process_unhandled_input(true);
}

void RNGameServices::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			Input::get_singleton()->connect("joy_connection_changed", callable_mp(this, &RNGameServices::_joy_connection_changed));
			AudioServer::get_singleton()->connect("bus_layout_changed", callable_mp(this, &RNGameServices::_buses_changed));
		} break;
		case NOTIFICATION_EXIT_TREE: {
			// Polyphonic voices keep their streams until they stop.
			stop_audio(StringName());
			Input::get_singleton()->disconnect("joy_connection_changed", callable_mp(this, &RNGameServices::_joy_connection_changed));
			AudioServer::get_singleton()->disconnect("bus_layout_changed", callable_mp(this, &RNGameServices::_buses_changed));
		} break;
		case NOTIFICATION_APPLICATION_FOCUS_OUT:
		case NOTIFICATION_WM_WINDOW_FOCUS_OUT: {
			_release_pressed_actions();
		} break;
		case NOTIFICATION_TRANSLATION_CHANGED: {
			if (translation_changed) {
				translation_changed();
			}
		} break;
	}
}

void RNGameServices::set_input_callbacks(ActionCallback p_action, DeviceCallback p_device, JoypadsCallback p_joypads) {
	action_changed = std::move(p_action);
	device_changed = std::move(p_device);
	joypads_changed = std::move(p_joypads);
}

void RNGameServices::watch_action(const StringName &p_action) {
	++watched_actions[p_action];
}

void RNGameServices::unwatch_action(const StringName &p_action) {
	int *count = watched_actions.getptr(p_action);
	if (count && --*count <= 0) {
		watched_actions.erase(p_action);
		pressed_actions.erase(p_action);
	}
}

void RNGameServices::capture_next_event(const PackedStringArray &p_devices, CaptureCallback p_finished) {
	cancel_capture();
	capturing = true;
	capture_devices = p_devices;
	capture_finished = std::move(p_finished);
}

void RNGameServices::cancel_capture() {
	if (capturing) {
		capturing = false;
		const CaptureCallback finished = std::move(capture_finished);
		capture_finished = nullptr;
		finished(Ref<InputEvent>());
	}
}

void RNGameServices::input(const Ref<InputEvent> &p_event) {
	const String device = rn_input_device_of(p_event);
	if (!device.is_empty() && device != last_device) {
		last_device = device;
		if (device_changed) {
			device_changed(device);
		}
	}
	if (!capturing || device.is_empty() || !p_event->is_pressed() || p_event->is_echo() || !capture_devices.has(device)) {
		return;
	}
	get_viewport()->set_input_as_handled();
	capturing = false;
	const CaptureCallback finished = std::move(capture_finished);
	capture_finished = nullptr;
	finished(p_event);
}

void RNGameServices::unhandled_input(const Ref<InputEvent> &p_event) {
	if (!action_changed || watched_actions.is_empty()) {
		return;
	}
	for (const KeyValue<StringName, int> &watched : watched_actions) {
		if (!InputMap::get_singleton()->has_action(watched.key) || !p_event->is_action(watched.key)) {
			continue;
		}
		// Input tracks every event bound to the action. This event alone can't say:
		// a stick's small move reads as not pressed while a key still holds the action.
		const bool pressed = Input::get_singleton()->is_action_pressed(watched.key);
		const bool echo = pressed && p_event->is_echo();
		bool *previous = pressed_actions.getptr(watched.key);
		if ((previous ? *previous : false) == pressed && !echo) {
			continue;
		}
		pressed_actions[watched.key] = pressed;
		action_changed(watched.key, pressed, Input::get_singleton()->get_action_strength(watched.key), echo);
	}
}

// Godot forgets pressed keys when focus leaves, without sending their releases.
void RNGameServices::_release_pressed_actions() {
	Vector<StringName> released;
	for (const KeyValue<StringName, bool> &action : pressed_actions) {
		if (action.value) {
			released.push_back(action.key);
		}
	}
	pressed_actions.clear();
	if (!action_changed) {
		return;
	}
	for (const StringName &action : released) {
		action_changed(action, false, 0, false);
	}
}

void RNGameServices::_joy_connection_changed(int, bool) {
	if (joypads_changed) {
		joypads_changed();
	}
}

void RNGameServices::_buses_changed() {
	if (buses_changed) {
		buses_changed();
	}
}

void RNGameServices::play_audio(const Ref<AudioStream> &p_stream, const StringName &p_bus, float p_volume_db, float p_pitch_scale) {
	AudioStreamPlayer **found = players.getptr(p_bus);
	AudioStreamPlayer *player = found ? *found : nullptr;
	if (!player) {
		Ref<AudioStreamPolyphonic> voices;
		voices.instantiate();
		voices->set_polyphony(GLOBAL_GET("react_native/audio/max_polyphony"));
		player = memnew(AudioStreamPlayer);
		player->set_stream(voices);
		player->set_bus(p_bus);
		add_child(player, false, INTERNAL_MODE_BACK);
		players.insert(p_bus, player);
	}
	if (!player->is_playing()) {
		player->play();
	}
	const Ref<AudioStreamPlaybackPolyphonic> playback = player->get_stream_playback();
	ERR_FAIL_COND(playback.is_null());
	playback->play_stream(p_stream, 0, p_volume_db, p_pitch_scale, AudioServer::PLAYBACK_TYPE_DEFAULT, p_bus);
}

void RNGameServices::stop_audio(const StringName &p_bus) {
	for (const KeyValue<StringName, AudioStreamPlayer *> &entry : players) {
		if (p_bus == StringName() || entry.key == p_bus) {
			entry.value->stop();
		}
	}
}

RichTextLabel *RNGameServices::get_measuring_label() {
	if (!measuring_label) {
		measuring_label = memnew(RichTextLabel);
		measuring_label->set_visible(false);
		measuring_label->set_use_bbcode(true);
		measuring_label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
		measuring_label->set_scroll_active(false);
		add_child(measuring_label, false, INTERNAL_MODE_BACK);
	}
	return measuring_label;
}

RNGameServices *RNGameServicesHandle::get() {
	if (freed) {
		return nullptr;
	}
	if (RNGameServices *existing = Object::cast_to<RNGameServices>(ObjectDB::get_instance(node))) {
		return existing;
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (!tree || !tree->get_root()) {
		return nullptr;
	}
	RNGameServices *services = memnew(RNGameServices);
	node = services->get_instance_id();
	tree->get_root()->add_child(services, false, Node::INTERNAL_MODE_BACK);
	return services;
}

void RNGameServicesHandle::free_node() {
	if (RNGameServices *services = Object::cast_to<RNGameServices>(ObjectDB::get_instance(node))) {
		services->queue_free();
	}
	node = ObjectID();
	freed = true;
}
