#pragma once

#include "core/templates/hash_map.h"
#include "core/variant/variant.h"
#include "scene/main/node.h"
#include "servers/audio/audio_stream.h"

#include <functional>
#include <memory>

class AudioStreamPlayer;
class RichTextLabel;

// What game-facing modules and components need from the scene tree: input
// callbacks, translation notifications, UI audio players and a label to measure
// BBCode with. One lives under the tree's root for each runtime. It keeps
// running while the tree is paused, because pause menus need it.
class RNGameServices : public Node {
	GDCLASS(RNGameServices, Node);

public:
	using ActionCallback = std::function<void(const String &p_action, bool p_pressed, float p_strength, bool p_echo)>;
	using DeviceCallback = std::function<void(const String &p_device)>;
	using JoypadsCallback = std::function<void()>;
	using TranslationCallback = std::function<void()>;
	using BusesCallback = std::function<void()>;
	using CaptureCallback = std::function<void(const Ref<InputEvent> &p_event)>;

private:
	struct PlayerKey {
		String stream;
		StringName bus;
		bool operator==(const PlayerKey &p_other) const { return stream == p_other.stream && bus == p_other.bus; }
		static uint32_t hash(const PlayerKey &p_key) { return p_key.stream.hash() ^ String(p_key.bus).hash(); }
	};
	HashMap<StringName, int> watched_actions;
	HashMap<StringName, bool> pressed_actions;
	String last_device = "keyboard";
	bool capturing = false;
	PackedStringArray capture_devices;
	CaptureCallback capture_finished;
	ActionCallback action_changed;
	DeviceCallback device_changed;
	JoypadsCallback joypads_changed;
	TranslationCallback translation_changed;
	BusesCallback buses_changed;
	HashMap<PlayerKey, AudioStreamPlayer *, PlayerKey> players;
	RichTextLabel *measuring_label = nullptr;

	void _joy_connection_changed(int p_device, bool p_connected);
	void _buses_changed();

protected:
	static void _bind_methods() {}
	void _notification(int p_what);
	void input(const Ref<InputEvent> &p_event) override;
	void unhandled_input(const Ref<InputEvent> &p_event) override;

public:
	RNGameServices();

	void set_input_callbacks(ActionCallback p_action, DeviceCallback p_device, JoypadsCallback p_joypads);
	void watch_action(const StringName &p_action);
	void unwatch_action(const StringName &p_action);
	String get_last_device() const { return last_device; }
	// A capture in progress ends without an event when another one starts.
	void capture_next_event(const PackedStringArray &p_devices, CaptureCallback p_finished);
	void cancel_capture();

	void set_translation_callback(TranslationCallback p_changed) { translation_changed = std::move(p_changed); }

	void set_buses_callback(BusesCallback p_changed) { buses_changed = std::move(p_changed); }
	void notify_buses_changed() { _buses_changed(); }
	void play_audio(const String &p_path, const Ref<AudioStream> &p_stream, const StringName &p_bus, float p_volume_db, float p_pitch_scale);
	void stop_audio(const StringName &p_bus);

	RichTextLabel *get_measuring_label();
};

// Creates the node on first use and frees it with its runtime. Main thread only.
class RNGameServicesHandle {
	ObjectID node;
	bool freed = false;

public:
	RNGameServices *get();
	void free_node();
};

// The device class of an event, or empty for events that say nothing about it.
String rn_input_device_of(const Ref<InputEvent> &p_event);
