#pragma once

#include "../mounting/rn_component_view.h"

#include "scene/gui/control.h"

#include <functional>

// Shows one instance of a PackedScene loaded on a resource-loader thread.
// Controls fill the host; other nodes, such as Node2D or Node3D scenes, are
// shown through a SubViewport that the host's size stretches.
class RNGodotSceneHost : public Control {
	GDCLASS(RNGodotSceneHost, Control);

	String source;
	String loading;
	Node *instance = nullptr;
	std::function<void(Node *)> loaded;
	std::function<void(const String &)> failed;

	void _poll();
	void _show(Node *p_instance);
	void _clear();

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	RNGodotSceneHost();
	void set_callbacks(std::function<void(Node *p_instance)> p_loaded, std::function<void(const String &p_message)> p_failed);
	void set_source(const String &p_source);
	Node *get_instance() const { return instance; }
};

class RNGodotSceneComponentView : public RNComponentView {
	RNGodotSceneHost *host;
	facebook::react::EventEmitter::Shared emitter;

	void _loaded(Node *p_instance);
	void _failed(const String &p_message);

public:
	explicit RNGodotSceneComponentView(facebook::react::Tag p_tag);
	~RNGodotSceneComponentView() override;
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
	Node *get_native_node() const override { return host->get_instance(); }
};
