#include "../components/rn_scroll_component_view.h"
#include "../components/rn_view_component_view.h"
#include "../mounting/rn_component_view_registry.h"
#include "support/rn_upstream_test_macros.h"

#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

#include <react/renderer/components/scrollview/ScrollViewProps.h>
#include <react/renderer/graphics/Color.h>

namespace TestRNComponentViews {

using namespace facebook::react;

ShadowView view(Tag p_tag, const Rect &p_frame, SharedColor p_background = {}) {
	auto props = std::make_shared<ViewProps>();
	props->backgroundColor = p_background;
	ShadowView result;
	result.componentName = "View";
	result.tag = p_tag;
	result.props = props;
	result.layoutMetrics.frame = p_frame;
	return result;
}

::Vector<Tag> react_order(Control *p_container) {
	::Vector<Tag> tags;
	for (int i = 0; i < p_container->get_child_count(); ++i) {
		if (p_container->get_child(i)->has_meta("_react_native_tag")) {
			tags.push_back(int(p_container->get_child(i)->get_meta("_react_native_tag")));
		}
	}
	return tags;
}

TEST_CASE("[ReactNativeBindings][ComponentViews][SceneTree] mutations keep identity, React order and authored children") {
	Control *container = memnew(Control);
	SceneTree::get_singleton()->get_root()->add_child(container);
	Control *authored = memnew(Control);
	container->add_child(authored);
	auto factories = std::make_shared<RNComponentViewFactories>();
	factories->add("View", [](const ShadowView &p_view) { return std::make_unique<RNViewComponentView>(p_view.tag); });
	RNRootComponentView root(1, container);
	RNComponentViewRegistry registry(factories);
	registry.register_root(1, &root);

	const ShadowView first = view(2, { .origin = { .x = 4, .y = 6 }, .size = { .width = 40, .height = 20 } });
	const ShadowView second = view(3, { .origin = { .x = 0, .y = 30 }, .size = { .width = 10, .height = 10 } });
	registry.apply({
			ShadowViewMutation::CreateMutation(first),
			ShadowViewMutation::CreateMutation(second),
			ShadowViewMutation::InsertMutation(1, first, 0),
			ShadowViewMutation::InsertMutation(1, second, 1),
	});
	Control *first_control = registry.find(2)->get_control();
	CHECK(container->get_child(0) == authored);
	CHECK(react_order(container) == ::Vector<Tag>({ 2, 3 }));
	CHECK(first_control->get_position() == Point2(4, 6));
	CHECK(first_control->get_size() == Size2(40, 20));

	registry.apply({
			ShadowViewMutation::RemoveMutation(1, first, 0),
			ShadowViewMutation::InsertMutation(1, first, 1),
	});
	CHECK(react_order(container) == ::Vector<Tag>({ 3, 2 }));
	CHECK(registry.find(2)->get_control() == first_control);

	const ShadowView recolored = view(2, first.layoutMetrics.frame, colorFromComponents({ .red = 1, .green = 0, .blue = 0, .alpha = 1 }));
	registry.apply({ ShadowViewMutation::UpdateMutation(first, recolored, 1) });
	const Ref<StyleBoxFlat> style = first_control->get_theme_stylebox("panel");
	REQUIRE(style.is_valid());
	CHECK(style->get_bg_color().is_equal_approx(::Color(1, 0, 0, 1)));

	const ShadowView nested = view(4, { .origin = { .x = 1, .y = 1 }, .size = { .width = 2, .height = 2 } });
	registry.apply({ ShadowViewMutation::CreateMutation(nested), ShadowViewMutation::InsertMutation(2, nested, 0) });
	CHECK(registry.find(4)->get_control()->get_parent() == first_control);

	registry.apply({
			ShadowViewMutation::RemoveMutation(1, second, 0),
			ShadowViewMutation::DeleteMutation(second),
	});
	CHECK(registry.find(3) == nullptr);
	CHECK(react_order(container) == ::Vector<Tag>({ 2 }));
	CHECK(container->get_child(0) == authored);

	registry.clear();
	memdelete(container);
}

TEST_CASE("[ReactNativeBindings][ComponentViews][SceneTree] unknown components mount a fallback and report once") {
	Control *container = memnew(Control);
	auto factories = std::make_shared<RNComponentViewFactories>();
	factories->set_fallback([](const ShadowView &p_view) { return std::make_unique<RNViewComponentView>(p_view.tag); });
	CHECK_FALSE(factories->has("Mystery"));
	RNRootComponentView root(1, container);
	RNComponentViewRegistry registry(factories);
	registry.register_root(1, &root);
	ShadowView mystery = view(5, { .size = { .width = 3, .height = 3 } });
	mystery.componentName = "Mystery";
	ERR_PRINT_OFF;
	registry.apply({ ShadowViewMutation::CreateMutation(mystery), ShadowViewMutation::InsertMutation(1, mystery, 0) });
	ERR_PRINT_ON;
	REQUIRE(registry.find(5) != nullptr);
	CHECK(registry.find(5)->get_control()->get_parent() == container);
	registry.clear();
	memdelete(container);
}

struct Surface {
	Control *container = memnew(Control);
	std::shared_ptr<RNComponentViewFactories> factories = std::make_shared<RNComponentViewFactories>();
	std::unique_ptr<RNRootComponentView> root;
	std::unique_ptr<RNComponentViewRegistry> registry;

	Surface() {
		SceneTree::get_singleton()->get_root()->add_child(container);
		factories->add("View", [](const ShadowView &p_view) { return std::make_unique<RNViewComponentView>(p_view.tag); });
		factories->add("ScrollView", [](const ShadowView &p_view) { return std::make_unique<RNScrollComponentView>(p_view.tag); });
		root = std::make_unique<RNRootComponentView>(1, container);
		registry = std::make_unique<RNComponentViewRegistry>(factories);
		registry->register_root(1, root.get());
	}
	~Surface() {
		registry.reset();
		root.reset();
		memdelete(container);
	}
};

ShadowView with_props(ShadowView p_view, const std::function<void(ViewProps &)> &p_change) {
	auto props = std::make_shared<ViewProps>(static_cast<const ViewProps &>(*p_view.props));
	p_change(*props);
	p_view.props = props;
	return p_view;
}

TEST_CASE("[ReactNativeBindings][ComponentViews][SceneTree] a move inside one transaction keeps the focused control") {
	Surface surface;
	const ShadowView first = with_props(view(2, { .size = { .width = 10, .height = 10 } }), [](ViewProps &p_props) { p_props.focusable = true; });
	const ShadowView second = view(3, { .size = { .width = 10, .height = 10 } });
	surface.registry->apply({
			ShadowViewMutation::CreateMutation(first),
			ShadowViewMutation::CreateMutation(second),
			ShadowViewMutation::InsertMutation(1, first, 0),
			ShadowViewMutation::InsertMutation(1, second, 1),
	});
	Control *focused = surface.registry->find(2)->get_control();
	focused->grab_focus();
	REQUIRE(focused->has_focus());
	surface.registry->apply({
			ShadowViewMutation::RemoveMutation(1, first, 0),
			ShadowViewMutation::InsertMutation(1, first, 1),
	});
	CHECK(react_order(surface.container) == ::Vector<Tag>({ 3, 2 }));
	CHECK(focused->has_focus());
}

TEST_CASE("[ReactNativeBindings][ComponentViews][SceneTree] a child moved out of a parent deleted in the same transaction survives") {
	Surface surface;
	const ShadowView parent = view(2, { .size = { .width = 20, .height = 20 } });
	const ShadowView child = view(3, { .size = { .width = 5, .height = 5 } });
	surface.registry->apply({
			ShadowViewMutation::CreateMutation(parent),
			ShadowViewMutation::CreateMutation(child),
			ShadowViewMutation::InsertMutation(1, parent, 0),
			ShadowViewMutation::InsertMutation(2, child, 0),
	});
	Control *child_control = surface.registry->find(3)->get_control();
	surface.registry->apply({
			ShadowViewMutation::RemoveMutation(2, child, 0),
			ShadowViewMutation::RemoveMutation(1, parent, 0),
			ShadowViewMutation::DeleteMutation(parent),
			ShadowViewMutation::InsertMutation(1, child, 0),
	});
	REQUIRE(surface.registry->find(3) != nullptr);
	CHECK(ObjectDB::get_instance(child_control->get_instance_id()) == child_control);
	CHECK(child_control->get_parent() == surface.container);
}

TEST_CASE("[ReactNativeBindings][ComponentViews][SceneTree] transforms use the offset transform and report skew once") {
	Surface surface;
	const ShadowView moved = with_props(view(2, { .origin = { .x = 4, .y = 6 }, .size = { .width = 10, .height = 10 } }), [](ViewProps &p_props) {
		p_props.transform = Transform::Translate(10, 20, 0) * Transform::Scale(2, 3, 1);
	});
	surface.registry->apply({ ShadowViewMutation::CreateMutation(moved), ShadowViewMutation::InsertMutation(1, moved, 0) });
	Control *control = surface.registry->find(2)->get_control();
	CHECK(control->get_position() == Point2(4, 6));
	CHECK(control->is_offset_transform_enabled());
	CHECK(control->get_offset_transform_scale().is_equal_approx(Vector2(2, 3)));

	const ShadowView skewed = with_props(moved, [](ViewProps &p_props) { p_props.transform = Transform::Skew(0.5, 0); });
	ERR_PRINT_OFF;
	surface.registry->apply({ ShadowViewMutation::UpdateMutation(moved, skewed, 0) });
	ERR_PRINT_ON;
	CHECK(control->has_meta("_react_native_skew_reported"));

	const ShadowView plain = with_props(moved, [](ViewProps &p_props) { p_props.transform = Transform::Identity(); });
	surface.registry->apply({ ShadowViewMutation::UpdateMutation(skewed, plain, 0) });
	CHECK_FALSE(control->is_offset_transform_enabled());
	CHECK(control->get_position() == Point2(4, 6));
}

TEST_CASE("[ReactNativeBindings][ComponentViews][SceneTree] input controls resolve to the view that owns them") {
	Surface surface;
	const ShadowView scroll = view(2, { .size = { .width = 50, .height = 50 } });
	ShadowView scroll_view = scroll;
	scroll_view.componentName = "ScrollView";
	scroll_view.props = std::make_shared<ScrollViewProps>();
	const ShadowView item = view(3, { .size = { .width = 10, .height = 10 } });
	surface.registry->apply({
			ShadowViewMutation::CreateMutation(scroll_view),
			ShadowViewMutation::CreateMutation(item),
			ShadowViewMutation::InsertMutation(1, scroll_view, 0),
			ShadowViewMutation::InsertMutation(2, item, 0),
	});
	auto *scroll_component = static_cast<RNScrollComponentView *>(surface.registry->find(2));
	CHECK(surface.registry->find_for_control(scroll_component->get_scroll_control()->get_v_scroll_bar()) == scroll_component);
	CHECK(surface.registry->find_for_control(surface.registry->find(3)->get_control()) == surface.registry->find(3));
	Control *authored = memnew(Control);
	surface.registry->find(3)->get_control()->add_child(authored);
	CHECK(surface.registry->find_for_control(authored) == nullptr);
}

TEST_CASE("[ReactNativeBindings][ComponentViews][SceneTree] pointerEvents none disables the whole Godot subtree") {
	Surface surface;
	const ShadowView parent = with_props(view(2, { .size = { .width = 20, .height = 20 } }), [](ViewProps &p_props) { p_props.pointerEvents = PointerEventsMode::None; });
	const ShadowView child = view(3, { .size = { .width = 5, .height = 5 } });
	surface.registry->apply({
			ShadowViewMutation::CreateMutation(parent),
			ShadowViewMutation::CreateMutation(child),
			ShadowViewMutation::InsertMutation(1, parent, 0),
			ShadowViewMutation::InsertMutation(2, child, 0),
	});
	Control *child_control = surface.registry->find(3)->get_control();
	CHECK(child_control->get_mouse_filter_with_override() == Control::MOUSE_FILTER_IGNORE);
	const ShadowView enabled = with_props(parent, [](ViewProps &p_props) { p_props.pointerEvents = PointerEventsMode::Auto; });
	surface.registry->apply({ ShadowViewMutation::UpdateMutation(parent, enabled, 0) });
	CHECK(child_control->get_mouse_filter_with_override() == Control::MOUSE_FILTER_PASS);
}

} // namespace TestRNComponentViews

void rn_force_link_component_view_tests() {}
