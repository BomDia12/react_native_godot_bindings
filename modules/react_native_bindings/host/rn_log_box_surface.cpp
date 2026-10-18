#include "rn_log_box_surface.h"

#include "../root_view/react_native_root_view.h"
#include "rn_main_thread_queue.h"

#include "scene/main/canvas_layer.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/rendering/rendering_server.h"

RNLogBoxSurface::RNLogBoxSurface(std::shared_ptr<RNMainThreadQueue> p_main_thread) :
		main_thread(std::move(p_main_thread)) {
}

void RNLogBoxSurface::createContentView(std::string p_application_key) {
	application_key = std::move(p_application_key);
	ready = true;
}

void RNLogBoxSurface::destroyContentView() {
	ready = false;
	hide();
}

void RNLogBoxSurface::show() {
	showing = true;
	main_thread->post([this, key = application_key] { _open(key); });
}

void RNLogBoxSurface::hide() {
	showing = false;
	main_thread->post([this] { close(); });
}

void RNLogBoxSurface::_open(const std::string &p_application_key) {
	SceneTree *tree = SceneTree::get_singleton();
	if (ObjectDB::get_instance(layer) || !tree || !tree->get_root()) {
		return;
	}
	CanvasLayer *canvas = memnew(CanvasLayer);
	canvas->set_layer(RSE::CANVAS_LAYER_MAX);
	ReactNativeRootView *root = memnew(ReactNativeRootView);
	root->set_application_key(String::utf8(p_application_key.c_str()));
	root->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	canvas->add_child(root);
	tree->get_root()->add_child(canvas, false, Node::INTERNAL_MODE_BACK);
	layer = canvas->get_instance_id();
}

void RNLogBoxSurface::close() {
	if (Node *canvas = Object::cast_to<Node>(ObjectDB::get_instance(layer))) {
		canvas->get_parent()->remove_child(canvas);
		memdelete(canvas);
	}
	layer = ObjectID();
}
