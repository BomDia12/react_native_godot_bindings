#include "../components/rn_image_control.h"
#include "../components/rn_presentation_control.h"
#include "../components/rn_small_controls.h"
#include "../components/rn_text_control.h"
#include "../components/rn_text_input_control.h"
#include "../components/rn_view_component_view.h"
#include "support/rn_test_accessibility_server.h"
#include "support/rn_test_display_server.h"
#include "support/rn_upstream_test_macros.h"

#include "scene/gui/panel.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/theme/theme_db.h"

#include <react/renderer/components/view/ViewEventEmitter.h>
#include <react/renderer/core/EventDispatcher.h>
#include <react/renderer/runtimescheduler/RuntimeScheduler.h>

namespace TestRNAccessibility {

using namespace facebook::react;
using Role = AccessibilityServerEnums::AccessibilityRole;

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
	bool has_flag(const Node *p_node, AccessibilityServerEnums::AccessibilityFlags p_flag) const { return server->has_flag(p_node->get_accessibility_element(), p_flag); }
	bool perform(const Node *p_node, AccessibilityServerEnums::AccessibilityAction p_action, const Variant &p_data = Variant()) { return server->perform(p_node->get_accessibility_element(), p_action, p_data); }
	const RNTestAccessibilityServer::Element *find(const String &p_name) const { return server->find(p_name); }
};

// A mounted View with the given accessibility props.
struct MountedView {
	RNViewComponentView view{ 2 };

	explicit MountedView(const std::function<void(ViewProps &)> &p_change) {
		auto props = std::make_shared<ViewProps>();
		p_change(*props);
		view.update_props(nullptr, props);
		LayoutMetrics metrics;
		metrics.frame.size = { .width = 40, .height = 20 };
		view.update_layout(metrics);
		SceneTree::get_singleton()->get_root()->add_child(view.get_control());
	}
	RNViewControl *control() const { return Object::cast_to<RNViewControl>(view.get_control()); }
};

// A real event emitter whose dispatcher records each event type and stops it
// before it would need a JavaScript runtime.
struct RecordedEvents {
	facebook::react::RuntimeScheduler scheduler{ [](std::function<void(facebook::jsi::Runtime &)> &&) {} };
	std::shared_ptr<EventDispatcher> dispatcher;
	std::shared_ptr<const EventListener> listener;
	std::vector<std::string> types;

	RecordedEvents() {
		EventQueueProcessor processor([](facebook::jsi::Runtime &, EventTarget *, const std::string &, ReactEventPriority, const EventPayload &, HighResTimeStamp) {}, [](facebook::jsi::Runtime &) {}, [](const StateUpdate &) {}, {});
		dispatcher = std::make_shared<EventDispatcher>(processor, std::make_unique<EventBeat>(std::make_shared<EventBeat::OwnerBox>(), scheduler), [](const StateUpdate &) {}, std::weak_ptr<EventLogger>());
		listener = std::make_shared<const EventListener>([this](const RawEvent &p_event) {
			types.push_back(p_event.type);
			return true;
		});
		dispatcher->addListener(listener);
	}
	EventEmitter::Shared emitter() const { return std::make_shared<ViewEventEmitter>(nullptr, dispatcher); }
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

TEST_CASE("[ReactNativeBindings][Accessibility] React Native roles map to Godot roles") {
	const std::pair<const char *, Role> roles[] = {
		{ "button", Role::ROLE_BUTTON },
		{ "togglebutton", Role::ROLE_BUTTON },
		{ "link", Role::ROLE_LINK },
		{ "checkbox", Role::ROLE_CHECK_BOX },
		{ "radio", Role::ROLE_RADIO_BUTTON },
		{ "switch", Role::ROLE_CHECK_BUTTON },
		{ "adjustable", Role::ROLE_SLIDER },
		{ "slider", Role::ROLE_SLIDER },
		{ "spinbutton", Role::ROLE_SPIN_BUTTON },
		{ "progressbar", Role::ROLE_PROGRESS_INDICATOR },
		{ "scrollbar", Role::ROLE_SCROLL_BAR },
		{ "image", Role::ROLE_IMAGE },
		{ "img", Role::ROLE_IMAGE },
		{ "text", Role::ROLE_STATIC_TEXT },
		{ "summary", Role::ROLE_STATIC_TEXT },
		{ "search", Role::ROLE_TEXT_FIELD },
		{ "list", Role::ROLE_LIST },
		{ "listitem", Role::ROLE_LIST_ITEM },
		{ "grid", Role::ROLE_TABLE },
		{ "table", Role::ROLE_TABLE },
		{ "row", Role::ROLE_ROW },
		{ "cell", Role::ROLE_CELL },
		{ "gridcell", Role::ROLE_CELL },
		{ "rowheader", Role::ROLE_ROW_HEADER },
		{ "columnheader", Role::ROLE_COLUMN_HEADER },
		{ "menu", Role::ROLE_MENU },
		{ "menubar", Role::ROLE_MENU_BAR },
		{ "menuitem", Role::ROLE_MENU_ITEM },
		{ "tab", Role::ROLE_TAB },
		{ "tablist", Role::ROLE_TAB_BAR },
		{ "tabbar", Role::ROLE_TAB_BAR },
		{ "tabpanel", Role::ROLE_TAB_PANEL },
		{ "dialog", Role::ROLE_DIALOG },
		{ "alertdialog", Role::ROLE_DIALOG },
		{ "toolbar", Role::ROLE_REGION },
		{ "group", Role::ROLE_REGION },
		{ "region", Role::ROLE_REGION },
		{ "tooltip", Role::ROLE_TOOLTIP },
		{ "tree", Role::ROLE_TREE },
		{ "treeitem", Role::ROLE_TREE_ITEM },
		{ "combobox", Role::ROLE_LIST_BOX },
		{ "option", Role::ROLE_LIST_BOX_OPTION },
	};
	String description;
	for (const auto &[name, role] : roles) {
		CHECK_MESSAGE(rn_accessibility_role(name, description) == role, name);
		CHECK(description.is_empty());
	}
	for (const char *landmark : { "header", "heading", "alert", "timer", "keyboardkey", "imagebutton", "article", "banner", "main", "navigation" }) {
		CHECK(rn_accessibility_role(landmark, description) == Role::ROLE_CONTAINER);
		CHECK(description == landmark);
	}
}

TEST_CASE("[ReactNativeBindings][Accessibility][SceneTree] views publish labels, states, values, hidden subtrees, modals and live regions") {
	ScreenReader reader;
	MountedView mounted([](ViewProps &p_props) {
		p_props.accessibilityLabel = "Volume";
		p_props.accessibilityHint = "Adjusts the music";
		p_props.accessibilityLiveRegion = AccessibilityLiveRegion::Polite;
		p_props.role = facebook::react::Role::Slider;
		p_props.accessibilityState = AccessibilityState{ .disabled = true, .selected = true, .busy = true, .expanded = false, .checked = AccessibilityState::Mixed };
		p_props.accessibilityValue = AccessibilityValue{ .min = 0, .max = 10, .now = 4, .text = std::string("four") };
		p_props.accessibilityElementsHidden = true;
		p_props.accessibilityViewIsModal = true;
	});
	reader.flush();
	const RNTestAccessibilityServer::Element &element = reader.element(mounted.control());
	CHECK(element.name == "Volume");
	CHECK(element.description == "Adjusts the music");
	CHECK(element.live == AccessibilityServerEnums::LIVE_POLITE);
	CHECK(element.role == Role::ROLE_SLIDER);
	CHECK(element.checked == std::optional<bool>(false));
	CHECK(element.state_description == "mixed");
	CHECK(element.selected == std::optional<bool>(true));
	CHECK(element.expanded == std::optional<bool>(false));
	CHECK(element.num_range == Vector2(0, 10));
	CHECK(element.num_value == 4);
	CHECK(element.value == "four");
	CHECK(reader.has_flag(mounted.control(), AccessibilityServerEnums::FLAG_DISABLED));
	CHECK(reader.has_flag(mounted.control(), AccessibilityServerEnums::FLAG_BUSY));
	CHECK(reader.has_flag(mounted.control(), AccessibilityServerEnums::FLAG_HIDDEN));
	CHECK(reader.has_flag(mounted.control(), AccessibilityServerEnums::FLAG_MODAL));

	MountedView landmark([](ViewProps &p_props) { p_props.accessibilityRole = "banner"; });
	reader.flush();
	CHECK(reader.element(landmark.control()).role == Role::ROLE_CONTAINER);
	CHECK(reader.element(landmark.control()).role_description == "banner");
}

TEST_CASE("[ReactNativeBindings][Accessibility][SceneTree] accessible groups are named by their text") {
	ScreenReader reader;
	MountedView group([](ViewProps &p_props) { p_props.accessible = true; });
	for (const char *part : { "Health", "80%" }) {
		RNTextControl *text = memnew(RNTextControl);
		text->set_content(part, nullptr, false);
		group.control()->add_child(text);
	}
	reader.flush();
	CHECK(reader.element(group.control()).name == "Health 80%");
}

TEST_CASE("[ReactNativeBindings][Accessibility][SceneTree] screen-reader actions reach React Native") {
	ScreenReader reader;
	MountedView mounted([](ViewProps &p_props) {
		p_props.role = facebook::react::Role::Button;
		p_props.accessibilityActions = { AccessibilityAction{ .name = "increment" }, AccessibilityAction{ .name = "longpress", .label = std::string("Inspect") }, AccessibilityAction{ .name = "share" } };
	});
	int clicks = 0;
	::Vector<String> actions;
	mounted.control()->set_accessibility_callbacks([&clicks]() { ++clicks; }, [&actions](const String &p_name) { actions.push_back(p_name); });
	reader.flush();
	const RNTestAccessibilityServer::Element &element = reader.element(mounted.control());
	CHECK(element.custom_actions.size() == 2);
	CHECK(element.custom_actions[0] == "Inspect");
	CHECK(reader.perform(mounted.control(), AccessibilityServerEnums::ACTION_CLICK));
	CHECK(reader.perform(mounted.control(), AccessibilityServerEnums::ACTION_INCREMENT));
	CHECK(reader.perform(mounted.control(), AccessibilityServerEnums::ACTION_CUSTOM, 1));
	CHECK(clicks == 1);
	CHECK(actions == ::Vector<String>({ "increment", "share" }));

	MountedView activating([](ViewProps &p_props) {
		p_props.accessible = true;
		p_props.accessibilityActions = { AccessibilityAction{ .name = "activate" } };
	});
	activating.control()->set_accessibility_callbacks([&clicks]() { ++clicks; }, [&actions](const String &p_name) { actions.push_back(p_name); });
	reader.flush();
	CHECK(reader.perform(activating.control(), AccessibilityServerEnums::ACTION_CLICK));
	CHECK(clicks == 1);
	CHECK(actions[actions.size() - 1] == "activate");
}

TEST_CASE("[ReactNativeBindings][Accessibility][SceneTree] the view's event emitter receives clicks and actions") {
	ScreenReader reader;
	RecordedEvents events;
	MountedView mounted([](ViewProps &p_props) {
		p_props.role = facebook::react::Role::Button;
		p_props.accessibilityActions = { AccessibilityAction{ .name = "increment" } };
	});
	mounted.view.update_event_emitter(events.emitter());
	reader.flush();
	CHECK(reader.perform(mounted.control(), AccessibilityServerEnums::ACTION_CLICK));
	CHECK(reader.perform(mounted.control(), AccessibilityServerEnums::ACTION_INCREMENT));
	REQUIRE(events.types.size() == 2);
	CHECK(events.types[0] == "topClick");
	CHECK(events.types[1] == "topAccessibilityAction");
}

TEST_CASE("[ReactNativeBindings][Accessibility][SceneTree] none and presentation roles keep the view's own role") {
	ScreenReader reader;
	MountedView none([](ViewProps &p_props) { p_props.accessibilityRole = "none"; });
	MountedView presentation([](ViewProps &p_props) { p_props.role = facebook::react::Role::Presentation; });
	reader.flush();
	for (const MountedView *view : { &none, &presentation }) {
		const RNTestAccessibilityServer::Element &element = reader.element(view->control());
		CHECK(element.role == Role::ROLE_PANEL);
		CHECK(element.role_description.is_empty());
		CHECK_FALSE(element.actions.has(AccessibilityServerEnums::ACTION_CLICK));
	}
}

TEST_CASE("[ReactNativeBindings][Accessibility][SceneTree] accessible groups are one element that follows its text") {
	ScreenReader reader;
	MountedView group([](ViewProps &p_props) { p_props.accessible = true; });
	RNTextControl *text = memnew(RNTextControl);
	text->set_content("80%", nullptr, false);
	group.control()->add_child(text);
	reader.flush();
	CHECK(reader.element(group.control()).name == "80%");
	CHECK(reader.element(group.control()).children.is_empty());
	text->set_content("75%", nullptr, false);
	reader.flush();
	CHECK(reader.element(group.control()).name == "75%");
}

TEST_CASE("[ReactNativeBindings][Accessibility][SceneTree] images and modal overlays publish their roles") {
	ScreenReader reader;
	RNImageControl *image = memnew(RNImageControl);
	image->set_accessibility_name("Potion");
	RNModalOverlay *overlay = memnew(RNModalOverlay);
	SceneTree::get_singleton()->get_root()->add_child(image);
	SceneTree::get_singleton()->get_root()->add_child(overlay);
	reader.flush();
	CHECK(reader.element(image).role == Role::ROLE_IMAGE);
	CHECK(reader.element(image).name == "Potion");
	CHECK(reader.element(overlay).role == Role::ROLE_DIALOG);
	CHECK(reader.has_flag(overlay, AccessibilityServerEnums::FLAG_MODAL));
	memdelete(image);
	memdelete(overlay);
}

TEST_CASE("[ReactNativeBindings][Accessibility][SceneTree] text selection uses indices into the whole shaped text") {
	ScreenReader reader;
	RNTextControl *text = memnew(RNTextControl);
	SceneTree::get_singleton()->get_root()->add_child(text);
	text->set_size(Size2(400, 60));
	text->set_content("first line\nsecond line", [](const Size2 &, RNParagraphLayout &r_layout) {
		RNParagraphLayout::Fragment fragment;
		fragment.text = "first line\nsecond line";
		fragment.font = ThemeDB::get_singleton()->get_fallback_font();
		r_layout.shape({ fragment }, RNParagraphLayout::Attributes()); }, true);
	text->set_selection_range(13, 17);
	reader.flush();
	const RNTestAccessibilityServer::Element &element = reader.element(text);
	REQUIRE(element.sub_elements.size() == 2);
	CHECK(element.selection_start == element.sub_elements[1]);
	CHECK(element.selection_start_char == 13);
	CHECK(element.selection_end_char == 17);
	memdelete(text);
}

TEST_CASE("[ReactNativeBindings][Accessibility][SceneTree] text publishes its role and selection") {
	ScreenReader reader;
	RNTextControl *text = memnew(RNTextControl);
	SceneTree::get_singleton()->get_root()->add_child(text);
	text->set_size(Size2(400, 40));
	text->set_content("selectable words", [](const Size2 &, RNParagraphLayout &r_layout) {
		RNParagraphLayout::Fragment fragment;
		fragment.text = "selectable words";
		fragment.font = ThemeDB::get_singleton()->get_fallback_font();
		r_layout.shape({ fragment }, RNParagraphLayout::Attributes()); }, true);
	text->set_accessibility_role(Role::ROLE_LINK);
	text->set_selection_range(11, 16);
	reader.flush();
	const RNTestAccessibilityServer::Element &element = reader.element(text);
	CHECK(element.role == Role::ROLE_LINK);
	CHECK(element.value == "selectable words");
	CHECK(element.selection_start.is_valid());
	CHECK(element.selection_start_char == 11);
	CHECK(element.selection_end_char == 16);
	memdelete(text);
}

TEST_CASE("[ReactNativeBindings][Accessibility][SceneTree] announcements and native roles use Godot's own") {
	ScreenReader reader;
	SceneTree::get_singleton()->get_root()->accessibility_announcement("Saved");
	reader.flush();
	const RNTestAccessibilityServer::Element *announcement = reader.find("Saved");
	REQUIRE(announcement != nullptr);
	CHECK(announcement->live == AccessibilityServerEnums::LIVE_ASSERTIVE);

	RNSwitchControl *toggle = memnew(RNSwitchControl);
	RNTextInputControl *input = memnew(RNTextInputControl);
	SceneTree::get_singleton()->get_root()->add_child(toggle);
	SceneTree::get_singleton()->get_root()->add_child(input);
	reader.flush();
	CHECK(reader.element(toggle).role == Role::ROLE_CHECK_BUTTON);
	CHECK(reader.element(input->get_editor()).role == Role::ROLE_TEXT_FIELD);
	memdelete(toggle);
	memdelete(input);
}

} // namespace TestRNAccessibility

void rn_force_link_accessibility_tests() {}
