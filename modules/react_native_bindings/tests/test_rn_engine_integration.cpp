#include "../components/rn_godot_scene_component_view.h"
#include "../components/rn_theme_metrics.h"
#include "../components/rn_view_component_view.h"
#include "../host/rn_animation_clock.h"
#include "../mounting/rn_component_view_registry.h"
#include "../root_view/react_native_root_view.h"
#include "../turbomodules/rn_godot_host_module.h"
#include "support/rn_upstream_test_macros.h"

#include "core/io/dir_access.h"
#include "core/io/resource_saver.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "scene/2d/node_2d.h"
#include "scene/gui/subviewport_container.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"

#include <thread>

namespace TestRNEngineIntegration {

using namespace facebook::react;

ShadowView view(Tag p_tag, std::function<void(ViewProps &)> p_configure = nullptr) {
	auto props = std::make_shared<ViewProps>();
	props->focusable = true;
	if (p_configure) {
		p_configure(*props);
	}
	ShadowView result;
	result.componentName = "View";
	result.tag = p_tag;
	result.props = props;
	result.layoutMetrics.frame = { .origin = {}, .size = { .width = 20, .height = 20 } };
	return result;
}

struct ViewTree {
	Control *container = memnew(Control);
	RNRootComponentView root{ 1, container };
	RNComponentViewRegistry registry{ [] {
		auto factories = std::make_shared<RNComponentViewFactories>();
		factories->add("View", [](const ShadowView &p_view) { return std::make_unique<RNViewComponentView>(p_view.tag); });
		return factories;
	}() };

	ViewTree() {
		SceneTree::get_singleton()->get_root()->add_child(container);
		registry.register_root(1, &root);
	}
	~ViewTree() {
		registry.clear();
		memdelete(container);
	}
	Control *control(Tag p_tag) const { return registry.find(p_tag)->get_focus_control(); }
};

TEST_CASE("[ReactNativeBindings][EngineIntegration][SceneTree] TV focus props become Godot focus neighbors once their targets mount") {
	ViewTree tree;
	const ShadowView first = view(2, [](ViewProps &p_props) {
		p_props.nextFocusDown = 3;
		p_props.nextFocusForward = 3;
	});
	tree.registry.apply({ ShadowViewMutation::CreateMutation(first), ShadowViewMutation::InsertMutation(1, first, 0) });
	CHECK(tree.control(2)->get_focus_neighbor(SIDE_BOTTOM).is_empty());

	const ShadowView second = view(3, [](ViewProps &p_props) { p_props.hasTVPreferredFocus = true; });
	tree.registry.apply({ ShadowViewMutation::CreateMutation(second), ShadowViewMutation::InsertMutation(1, second, 1) });
	CHECK(tree.control(2)->get_node_or_null(tree.control(2)->get_focus_neighbor(SIDE_BOTTOM)) == tree.control(3));
	CHECK(tree.control(2)->get_node_or_null(tree.control(2)->get_focus_next()) == tree.control(3));
	MessageQueue::get_singleton()->flush();
	CHECK(tree.control(3)->has_focus());

	const ShadowView cleared = view(2);
	tree.registry.apply({ ShadowViewMutation::UpdateMutation(first, cleared, 1) });
	CHECK(tree.control(2)->get_focus_neighbor(SIDE_BOTTOM).is_empty());
	CHECK(tree.control(2)->get_focus_next().is_empty());
}

TEST_CASE("[ReactNativeBindings][EngineIntegration] a root sized to its content reports React's size as its minimum") {
	ReactNativeRootView *root = memnew(ReactNativeRootView);
	const RNMountedSurface surface = root->_attach_surface(1, std::make_shared<RNComponentViewFactories>());
	ShadowView before;
	before.tag = 1;
	ShadowView after = before;
	after.layoutMetrics.frame.size = { .width = 120, .height = 48 };
	surface.views->update(before, after);
	CHECK(root->get_minimum_size() == Size2());
	root->set_fit_content(ReactNativeRootView::FIT_CONTENT_HEIGHT);
	CHECK(root->get_minimum_size() == Size2(0, 48));
	root->set_fit_content(ReactNativeRootView::FIT_CONTENT_BOTH);
	CHECK(root->get_minimum_size() == Size2(120, 48));
	root->_detach_surface();
	memdelete(root);
}

TEST_CASE("[ReactNativeBindings][EngineIntegration] surfaces read their own theme metrics and fall back to the shared ones") {
	RNThemeMetricsStore store;
	RNThemeMetrics shared;
	shared.text_font_size = 16;
	RNThemeMetrics large;
	large.text_font_size = 32;
	store.publish(shared);
	store.publish(11, large);
	CHECK(store.get(11)->text_font_size == 32);
	CHECK(store.get(21)->text_font_size == 16);
	CHECK(store.get()->text_font_size == 16);
	store.forget(11);
	CHECK(store.get(11)->text_font_size == 16);
}

TEST_CASE("[ReactNativeBindings][EngineIntegration] the animation clock stands still while paused") {
	rn_set_animation_clock_paused(true);
	const auto paused = rn_animation_clock_now();
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	CHECK(rn_animation_clock_now() == paused);
	rn_set_animation_clock_paused(false);
	CHECK(rn_animation_clock_now() - paused < std::chrono::milliseconds(15));
}

TEST_CASE("[ReactNativeBindings][EngineIntegration] root state changes reach every live listener") {
	RNGodotHostEvents events;
	int calls = 0;
	RNRootState received;
	events.add_listener([&](int p_root_tag, const RNRootState &p_state) {
		calls += p_root_tag == 11;
		received = p_state;
		return true;
	});
	events.add_listener([](int, const RNRootState &) { return false; });
	events.root_state_changed(11, { .visible = true, .processing = false, .focused = true });
	events.root_state_changed(11, { .visible = false, .processing = false, .focused = false });
	CHECK(calls == 2);
	CHECK(received == RNRootState{ .visible = false, .processing = false, .focused = false });
	CHECK_FALSE(events.is_back_handler_enabled());
	events.set_back_handler_enabled(true);
	CHECK(events.is_back_handler_enabled());
}

Node *load_scene(RNGodotSceneHost *p_host, Node *p_scene_root, const String &p_path) {
	Ref<PackedScene> scene;
	scene.instantiate();
	scene->pack(p_scene_root);
	memdelete(p_scene_root);
	// Test runs have no project, so nothing has created the user data directory yet.
	DirAccess::make_dir_recursive_absolute(OS::get_singleton()->get_user_data_dir());
	REQUIRE(ResourceSaver::save(scene, p_path) == OK);
	Node *loaded = nullptr;
	p_host->set_callbacks([&loaded](Node *p_instance) { loaded = p_instance; }, [](const String &p_message) { FAIL(p_message); });
	p_host->set_source(p_path);
	for (int attempt = 0; attempt < 500 && !loaded; ++attempt) {
		OS::get_singleton()->delay_usec(2000);
		p_host->notification(Node::NOTIFICATION_INTERNAL_PROCESS);
	}
	return loaded;
}

TEST_CASE("[ReactNativeBindings][EngineIntegration][SceneTree] GodotScene fills itself with a Control scene and shows other scenes through a SubViewport") {
	RNGodotSceneHost *host = memnew(RNGodotSceneHost);
	SceneTree::get_singleton()->get_root()->add_child(host);
	host->set_size(Size2(64, 32));

	Control *panel = memnew(Control);
	panel->set_name("Panel");
	Node *control_instance = load_scene(host, panel, "user://rn_test_control_scene.tscn");
	REQUIRE(Object::cast_to<Control>(control_instance));
	CHECK(control_instance->get_parent() == host);
	CHECK(Object::cast_to<Control>(control_instance)->get_size() == Size2(64, 32));

	Node2D *sprite = memnew(Node2D);
	sprite->set_name("Sprite");
	Node *node_instance = load_scene(host, sprite, "user://rn_test_node_scene.tscn");
	REQUIRE(Object::cast_to<Node2D>(node_instance));
	CHECK(Object::cast_to<SubViewport>(node_instance->get_parent()));
	CHECK(Object::cast_to<SubViewportContainer>(node_instance->get_parent()->get_parent())->is_stretch_enabled());
	CHECK(host->get_instance() == node_instance);
	CHECK(host->get_child_count(true) == 1);

	host->set_source(String());
	CHECK(host->get_instance() == nullptr);
	host->set_callbacks(nullptr, nullptr);
	memdelete(host);
}

} // namespace TestRNEngineIntegration

void rn_force_link_engine_integration_tests() {}
