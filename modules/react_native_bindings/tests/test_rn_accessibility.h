#pragma once

#include "support/rn_test_accessibility_server.h"
#include "support/rn_test_display_server.h"

#include "scene/gui/panel.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "tests/test_macros.h"

namespace TestRNAccessibility {

// Installs a screen reader that records what the scene tree publishes.
class ScreenReader {
	RNTestDisplayServer *display = RNTestDisplayServer::install();
	RNTestAccessibilityServer *server = memnew(RNTestAccessibilityServer);

public:
	ScreenReader() { display->screen_reader_active = 1; }
	~ScreenReader() {
		display->screen_reader_active = -1;
		memdelete(server);
	}

	// The scene tree rate-limits accessibility updates, and a node entering the
	// tree is the public way to force the next one.
	void flush() {
		Node *trigger = memnew(Node);
		SceneTree::get_singleton()->get_root()->add_child(trigger);
		SceneTree::get_singleton()->process(0);
		memdelete(trigger);
	}

	const RNTestAccessibilityServer::Element &element(const Node *p_node) const {
		const RNTestAccessibilityServer::Element *found = server->element(p_node->get_accessibility_element());
		REQUIRE(found != nullptr);
		return *found;
	}
};

TEST_CASE("[ReactNativeBindings][Accessibility][SceneTree] the recording server receives what a Panel publishes") {
	ScreenReader reader;
	Panel *panel = memnew(Panel);
	panel->set_accessibility_name("Inventory");
	SceneTree::get_singleton()->get_root()->add_child(panel);
	reader.flush();
	CHECK(reader.element(panel).role == AccessibilityServerEnums::ROLE_PANEL);
	CHECK(reader.element(panel).name == "Inventory");
	memdelete(panel);
}

} // namespace TestRNAccessibility
