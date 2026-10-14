#include "../components/rn_view_component_view.h"
#include "../mounting/rn_component_view_registry.h"
#include "support/rn_upstream_test_macros.h"

#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

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
	RNRootComponentView root(1, container);
	RNComponentViewRegistry registry;
	registry.register_factory("View", [](const ShadowView &p_view) { return std::make_unique<RNViewComponentView>(p_view.tag); });
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

	registry.unregister_root(1);
}

TEST_CASE("[ReactNativeBindings][ComponentViews][SceneTree] unknown components mount a fallback and report once") {
	Control *container = memnew(Control);
	RNRootComponentView root(1, container);
	RNComponentViewRegistry registry;
	registry.register_root(1, &root);
	registry.set_fallback_factory([](const ShadowView &p_view) { return std::make_unique<RNViewComponentView>(p_view.tag); });
	CHECK_FALSE(registry.has_component("Mystery"));
	ShadowView mystery = view(5, { .size = { .width = 3, .height = 3 } });
	mystery.componentName = "Mystery";
	ERR_PRINT_OFF;
	registry.apply({ ShadowViewMutation::CreateMutation(mystery), ShadowViewMutation::InsertMutation(1, mystery, 0) });
	ERR_PRINT_ON;
	REQUIRE(registry.find(5) != nullptr);
	CHECK(registry.find(5)->get_control()->get_parent() == container);
	registry.unregister_root(1);
}

} // namespace TestRNComponentViews

void rn_force_link_component_view_tests() {}
