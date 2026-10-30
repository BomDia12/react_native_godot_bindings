#include "rn_godot_scene_component_view.h"

#include "../host/react_native_host.h"
#include "../root_view/react_native_root_view.h"

#include "core/io/resource_loader.h"
#include "scene/gui/subviewport_container.h"
#include "scene/main/viewport.h"
#include "scene/resources/packed_scene.h"

#include <react/renderer/components/ReactNativeGodotSpec/EventEmitters.h>
#include <react/renderer/components/ReactNativeGodotSpec/Props.h>

using namespace facebook::react;

RNGodotSceneHost::RNGodotSceneHost() {
	set_mouse_filter(MOUSE_FILTER_PASS);
}

void RNGodotSceneHost::set_callbacks(std::function<void(Node *)> p_loaded, std::function<void(const String &)> p_failed) {
	loaded = std::move(p_loaded);
	failed = std::move(p_failed);
}

void RNGodotSceneHost::set_source(const String &p_source) {
	if (source == p_source) {
		return;
	}
	source = p_source;
	_cancel_load();
	_clear();
	if (source.is_empty()) {
		return;
	}
	if (ResourceLoader::load_threaded_request(source, "PackedScene") != OK) {
		failed(vformat("Could not load the scene '%s'.", source));
		return;
	}
	loading = source;
	set_process_internal(true);
}

void RNGodotSceneHost::_notification(int p_what) {
	if (p_what == NOTIFICATION_INTERNAL_PROCESS) {
		_poll();
	} else if (p_what == NOTIFICATION_PREDELETE) {
		_cancel_load();
	}
}

// The loader keeps a request's task and resource until they are collected,
// which the host does once the load finishes, so nothing waits for it here.
void RNGodotSceneHost::_cancel_load() {
	if (!loading.is_empty()) {
		if (ReactNativeHost *host_singleton = ReactNativeHost::get_singleton()) {
			host_singleton->collect_scene_load(loading);
		} else {
			ResourceLoader::load_threaded_get(loading);
		}
		loading = String();
	}
	set_process_internal(false);
}

void RNGodotSceneHost::_poll() {
	const ResourceLoader::ThreadLoadStatus status = ResourceLoader::load_threaded_get_status(loading);
	if (status == ResourceLoader::THREAD_LOAD_IN_PROGRESS) {
		return;
	}
	set_process_internal(false);
	const Ref<PackedScene> scene = ResourceLoader::load_threaded_get(loading);
	loading = String();
	Node *created = scene.is_valid() ? scene->instantiate() : nullptr;
	if (!created) {
		failed(vformat("'%s' is not a scene that can be instantiated.", source));
		return;
	}
	_show(created);
	loaded(created);
}

void RNGodotSceneHost::_show(Node *p_instance) {
	instance = p_instance->get_instance_id();
	if (Control *control = Object::cast_to<Control>(p_instance)) {
		add_child(control, false, INTERNAL_MODE_FRONT);
		control->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
		shown = control->get_instance_id();
		return;
	}
	SubViewportContainer *container = memnew(SubViewportContainer);
	container->set_mouse_filter(MOUSE_FILTER_PASS);
	container->set_stretch(true);
	container->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	SubViewport *viewport = memnew(SubViewport);
	viewport->set_transparent_background(true);
	// Any node of the scene, not only its root, can be 3D, and a shared world
	// would show it in the game's own 3D world.
	viewport->set_use_own_world_3d(true);
	viewport->add_child(p_instance);
	container->add_child(viewport);
	add_child(container, false, INTERNAL_MODE_FRONT);
	shown = container->get_instance_id();
}

void RNGodotSceneHost::_clear() {
	Node *node = Object::cast_to<Node>(ObjectDB::get_instance(shown));
	instance = ObjectID();
	shown = ObjectID();
	if (node && node->get_parent() == this) {
		remove_child(node);
		node->queue_free();
	}
}

RNGodotSceneComponentView::RNGodotSceneComponentView(Tag p_tag) :
		RNComponentView(p_tag, memnew(RNGodotSceneHost)), host(Object::cast_to<RNGodotSceneHost>(get_control())) {
	host->set_callbacks([this](Node *p_instance) { _loaded(p_instance); }, [this](const String &p_message) { _failed(p_message); });
}

RNGodotSceneComponentView::~RNGodotSceneComponentView() {
	if (has_control()) {
		host->set_callbacks(nullptr, nullptr);
	}
}

void RNGodotSceneComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	host->set_source(String::utf8(static_cast<const GodotSceneProps &>(*p_new).source.c_str()));
}

void RNGodotSceneComponentView::update_event_emitter(const EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	emitter = p_emitter;
}

// GDScript finds the instance through the nearest root, which hosts this view.
void RNGodotSceneComponentView::_loaded(Node *p_instance) {
	for (Node *node = host->get_parent(); node; node = node->get_parent()) {
		if (ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(node)) {
			root->emit_signal("scene_loaded", String::utf8(get_native_id().c_str()), p_instance);
			break;
		}
	}
	if (auto scene_emitter = std::static_pointer_cast<const GodotSceneEventEmitter>(emitter)) {
		scene_emitter->onLoad({});
	}
}

void RNGodotSceneComponentView::_failed(const String &p_message) {
	ERR_PRINT(p_message);
	if (auto scene_emitter = std::static_pointer_cast<const GodotSceneEventEmitter>(emitter)) {
		scene_emitter->onError({ .message = p_message.utf8().get_data() });
	}
}
