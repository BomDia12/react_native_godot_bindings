#include "rn_view_component_view.h"

#include "../interop/rn_dynamic.h"
#include "rn_resource_requests.h"

#include <react/renderer/components/view/ViewEventEmitter.h>
#include <react/renderer/components/view/accessibilityPropsConversions.h>

#include <algorithm>

RNViewComponentView::RNViewComponentView(facebook::react::Tag p_tag) :
		RNComponentView(p_tag, memnew(RNViewControl)), view(Object::cast_to<RNViewControl>(get_control())) {
}

RNViewComponentView::~RNViewComponentView() {
	RNResourceRequests::cancel(skin_request);
	RNResourceRequests::cancel(material_request);
	RNResourceRequests::cancel(drag_preview_request);
}

void RNViewComponentView::_load_drag_preview(const String &p_path) {
	if (p_path == drag_preview_path) {
		return;
	}
	drag_preview_path = p_path;
	RNResourceRequests::cancel(drag_preview_request);
	drag_preview_request = 0;
	drag_preview.unref();
	if (p_path.is_empty()) {
		return;
	}
	drag_preview_request = RNResourceRequests::request(p_path, "Texture2D", [this](const Ref<Resource> &p_resource, const String &p_error) {
		drag_preview_request = 0;
		drag_preview = p_resource;
		if (drag_preview.is_null()) {
			ERR_PRINT(vformat("E_RESOURCE: godotDragPreview on React tag %d: %s", get_tag(), p_error.is_empty() ? vformat("'%s' is not a texture.", drag_preview_path) : p_error));
		}
		_apply_drag();
	});
}

// Objects need a declared capability, which a drag has none of.
static bool carries_godot_object(const folly::dynamic &p_value) {
	if (p_value.isArray()) {
		return std::any_of(p_value.begin(), p_value.end(), carries_godot_object);
	}
	if (!p_value.isObject()) {
		return false;
	}
	for (const auto &[key, value] : p_value.items()) {
		if ((key == "$godot" && value == "Object") || carries_godot_object(value)) {
			return true;
		}
	}
	return false;
}

void RNViewComponentView::_apply_drag() {
	Dictionary data;
	const folly::dynamic &declared = props->godotDragData;
	if (declared.isObject()) {
		const folly::dynamic *type = declared.get_ptr("type");
		if (!type || !type->isString()) {
			ERR_PRINT(vformat("E_VALIDATION: godotDragData on React tag %d needs a string 'type'.", get_tag()));
		} else if (carries_godot_object(declared)) {
			ERR_PRINT(vformat("E_VALIDATION: godotDragData on React tag %d cannot carry Godot objects.", get_tag()));
		} else {
			data = rn_typed_variant_from_dynamic(declared);
		}
	}
	const Array drop_types = props->godotDropTypes.isArray() ? Array(rn_variant_from_dynamic(props->godotDropTypes)) : Array();
	view->set_drag(data, drag_preview, drop_types);
	if (effect) {
		effect->get_painter()->set_drag(data, drag_preview, drop_types);
	}
}

void RNViewComponentView::_load_skin(const String &p_path) {
	if (p_path == skin_path) {
		return;
	}
	skin_path = p_path;
	RNResourceRequests::cancel(skin_request);
	skin_request = 0;
	skin.unref();
	if (p_path.is_empty()) {
		return;
	}
	skin_request = RNResourceRequests::request(p_path, "StyleBox", [this](const Ref<Resource> &p_resource, const String &p_error) {
		skin_request = 0;
		skin = p_resource;
		if (skin.is_null()) {
			ERR_PRINT(vformat("E_RESOURCE: godotStyleBox on React tag %d: %s", get_tag(), p_error.is_empty() ? vformat("'%s' is not a StyleBox.", skin_path) : p_error));
		}
		_apply_style();
	});
}

void RNViewComponentView::_load_material(const String &p_path) {
	if (p_path == material_path) {
		return;
	}
	material_path = p_path;
	RNResourceRequests::cancel(material_request);
	material_request = 0;
	material.unref();
	if (p_path.is_empty()) {
		return;
	}
	material_request = RNResourceRequests::request(p_path, "ShaderMaterial", [this](const Ref<Resource> &p_resource, const String &p_error) {
		material_request = 0;
		const Ref<ShaderMaterial> shared = p_resource;
		if (shared.is_null()) {
			ERR_PRINT(vformat("E_RESOURCE: godotMaterial on React tag %d: %s", get_tag(), p_error.is_empty() ? vformat("'%s' is not a ShaderMaterial.", material_path) : p_error));
		} else {
			// Views using one resource keep their own uniform values.
			material = shared->duplicate();
			_apply_shader_parameters();
		}
		_apply_style();
		// The painter of a new effect host takes the view's drag props.
		_apply_drag();
	});
}

void RNViewComponentView::_apply_shader_parameters() {
	if (material.is_null() || !props->godotShaderParameters.isObject()) {
		return;
	}
	const Dictionary parameters = rn_typed_variant_from_dynamic(props->godotShaderParameters);
	for (const KeyValue<Variant, Variant> &parameter : parameters) {
		material->set_shader_parameter(parameter.key, parameter.value);
	}
}

RNAccessibility rn_view_accessibility(const facebook::react::ViewProps &p_props) {
	using namespace facebook::react;
	RNAccessibility result;
	std::string role = p_props.role != Role::None ? toString(p_props.role) : p_props.accessibilityRole;
	// Both mean "no role" in React Native.
	if (role == "none" || role == "presentation") {
		role.clear();
	}
	if (!role.empty()) {
		String description;
		result.role = rn_accessibility_role(String::utf8(role.c_str()), description);
		result.role_description = description;
	}
	if (const std::optional<AccessibilityState> &state = p_props.accessibilityState) {
		result.disabled = state->disabled;
		result.busy = state->busy;
		result.selected = state->selected;
		result.expanded = state->expanded;
		if (state->checked == AccessibilityState::Checked || state->checked == AccessibilityState::Unchecked) {
			result.checked = state->checked == AccessibilityState::Checked;
		}
		result.mixed = state->checked == AccessibilityState::Mixed;
	}
	const AccessibilityValue &value = p_props.accessibilityValue;
	if (value.min && value.max && value.now) {
		result.range = Vector3(*value.min, *value.max, *value.now);
	}
	if (value.text) {
		result.value_text = String::utf8(value.text->c_str());
	}
	result.hidden = p_props.accessibilityElementsHidden || p_props.importantForAccessibility == ImportantForAccessibility::NoHideDescendants;
	result.modal = p_props.accessibilityViewIsModal;
	result.grouped = p_props.accessible;
	result.clickable = p_props.accessible || !role.empty();
	for (const AccessibilityAction &action : p_props.accessibilityActions) {
		bool named = false;
		for (const RNNamedAccessibilityAction &known : RN_NAMED_ACCESSIBILITY_ACTIONS) {
			if (action.name == known.name) {
				result.*known.listed = true;
				named = true;
			}
		}
		if (!named) {
			result.custom_names.push_back(String::utf8(action.name.c_str()));
			result.custom_labels.push_back(String::utf8(action.label.value_or(action.name).c_str()));
		}
	}
	return result;
}

RNStyleBox::Paint rn_view_paint(const facebook::react::ViewProps &p_props, const facebook::react::LayoutMetrics &p_layout) {
	using namespace facebook::react;
	const BorderMetrics border = p_props.resolveBorderMetrics(p_layout);
	RNStyleBox::Paint paint;
	paint.border_box = rn_rounded_rect(Rect2(Point2(), Size2(p_layout.frame.size.width, p_layout.frame.size.height)), border.borderRadii);
	paint.widths[SIDE_LEFT] = border.borderWidths.left;
	paint.widths[SIDE_TOP] = border.borderWidths.top;
	paint.widths[SIDE_RIGHT] = border.borderWidths.right;
	paint.widths[SIDE_BOTTOM] = border.borderWidths.bottom;
	paint.colors[SIDE_LEFT] = rn_godot_color(border.borderColors.left, ::Color(0, 0, 0));
	paint.colors[SIDE_TOP] = rn_godot_color(border.borderColors.top, ::Color(0, 0, 0));
	paint.colors[SIDE_RIGHT] = rn_godot_color(border.borderColors.right, ::Color(0, 0, 0));
	paint.colors[SIDE_BOTTOM] = rn_godot_color(border.borderColors.bottom, ::Color(0, 0, 0));
	paint.styles[SIDE_LEFT] = border.borderStyles.left;
	paint.styles[SIDE_TOP] = border.borderStyles.top;
	paint.styles[SIDE_RIGHT] = border.borderStyles.right;
	paint.styles[SIDE_BOTTOM] = border.borderStyles.bottom;
	paint.background = rn_godot_color(p_props.backgroundColor);
	paint.images = p_props.backgroundImage;
	paint.sizes = p_props.backgroundSize;
	paint.positions = p_props.backgroundPosition;
	paint.repeats = p_props.backgroundRepeat;
	paint.shadows = p_props.boxShadow;
	paint.outline_color = rn_godot_color(p_props.outlineColor, ::Color(0, 0, 0));
	paint.outline_width = p_props.outlineWidth;
	paint.outline_offset = p_props.outlineOffset;
	paint.outline_style = p_props.outlineStyle;
	return paint;
}

Control *RNViewComponentView::get_child_container() {
	if (clip) {
		return clip;
	}
	return effect ? effect->get_painter() : view;
}

void RNViewComponentView::_restructure(bool p_effect, bool p_clip) {
	if (p_effect == (effect != nullptr) && p_clip == (clip != nullptr)) {
		return;
	}
	Control *previous = get_child_container();
	Vector<Control *> children;
	Control *focused = nullptr;
	for (int i = 0; i < previous->get_child_count(); ++i) {
		if (Control *child = Object::cast_to<Control>(previous->get_child(i))) {
			children.push_back(child);
			if (Control *owner = child->get_viewport()->gui_get_focus_owner(); owner && (owner == child || child->is_ancestor_of(owner))) {
				focused = owner;
			}
		}
	}
	for (Control *child : children) {
		previous->remove_child(child);
	}
	if (clip) {
		clip->get_parent()->remove_child(clip);
		memdelete(clip);
		clip = nullptr;
	}
	if (p_effect != (effect != nullptr)) {
		if (effect) {
			view->remove_child(effect);
			memdelete(effect);
			effect = nullptr;
		} else {
			effect = memnew(RNEffectHost);
			view->add_child(effect, false, Node::INTERNAL_MODE_FRONT);
			view->copy_drag_callbacks_to(effect->get_painter());
		}
	}
	if (p_clip) {
		clip = memnew(RNClipControl);
		Control *parent = effect ? effect->get_painter() : view;
		parent->add_child(clip, false, Node::INTERNAL_MODE_FRONT);
	}
	Control *container = get_child_container();
	for (Control *child : children) {
		container->add_child(child);
		rn_inherit_clip(child);
	}
	if (focused && focused->is_inside_tree()) {
		focused->grab_focus();
	}
}

void RNViewComponentView::_apply_style() {
	using namespace facebook::react;
	const RNStyleBox::Paint paint = rn_view_paint(*props, layout_metrics);
	const ::Vector<RNFilterStage> stages = rn_filter_stages(props->filter);
	const bool needs_effect = !stages.is_empty() || props->mixBlendMode != BlendMode::Normal || props->isolation == Isolation::Isolate || material.is_valid();
	const bool needs_clip = props->getClipsContentToBounds();
	_restructure(needs_effect, needs_clip);
	view->set_clip_contents(false);
	const StringName theme_type = String::utf8(props->godotThemeType.c_str());
	view->set_skin(effect ? Ref<StyleBox>() : skin, effect ? StringName() : theme_type);
	view->set_paint(paint, !effect);
	const Size2 size = paint.border_box.rect.size;
	if (effect) {
		// Filters spread the painted box, shadows and outline included, so their reaches add up.
		effect->configure(size, paint.extent() + rn_filter_extent(stages), stages, props->mixBlendMode, material);
		effect->get_painter()->set_skin(skin, theme_type);
		effect->get_painter()->set_paint(paint);
	}
	if (clip) {
		clip->set_size(size);
		RNRoundedRect shape = paint.padding_box();
		clip->set_shape(shape);
	}
}

static bool names_focus_neighbors(const facebook::react::ViewProps &p_props) {
	return p_props.nextFocusDown || p_props.nextFocusForward || p_props.nextFocusLeft || p_props.nextFocusRight || p_props.nextFocusUp;
}

void RNViewComponentView::update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	using facebook::react::PointerEventsMode;
	const auto is_drop_shadow = [](const facebook::react::FilterFunction &p_function) { return p_function.type == facebook::react::FilterType::DropShadow; };
	if (!reported_drop_shadows && std::count_if(props->filter.begin(), props->filter.end(), is_drop_shadow) > RN_MAX_DROP_SHADOWS) {
		reported_drop_shadows = true;
		WARN_PRINT(vformat("E_UNSUPPORTED_STYLE: React tag %d has more than %d drop-shadow filters; the later ones are ignored.", get_tag(), RN_MAX_DROP_SHADOWS));
	}
	const bool self_targetable = props->pointerEvents != PointerEventsMode::None && props->pointerEvents != PointerEventsMode::BoxNone;
	view->set_focus_mode(self_targetable && props->focusable ? Control::FOCUS_ALL : Control::FOCUS_NONE);
	view->set_menu_entries(self_targetable ? Array(rn_variant_from_dynamic(props->godotContextMenu)) : Array());
	view->set_accessibility(rn_view_accessibility(*props));
	view->set_tooltip_text(String::utf8(props->tooltip.c_str()));
	_load_skin(String::utf8(props->godotStyleBox.c_str()));
	_load_material(String::utf8(props->godotMaterial.c_str()));
	_apply_shader_parameters();
	const folly::dynamic *preview_uri = props->godotDragPreview.isObject() ? props->godotDragPreview.get_ptr("uri") : nullptr;
	_load_drag_preview(preview_uri && preview_uri->isString() ? String::utf8(preview_uri->getString().c_str()) : String());
	auto old = std::static_pointer_cast<const facebook::react::ViewProps>(p_old);
	preferred_focus_pending = props->hasTVPreferredFocus && (preferred_focus_pending || !(old && old->hasTVPreferredFocus));
	// Neighbors that GDScript set through the native node stay until React
	// names some of its own.
	if (!has_focus_references() && old && names_focus_neighbors(*old)) {
		resolve_focus_references([](facebook::react::Tag) { return nullptr; });
	}
	_apply_style();
	_apply_drag();
}

bool RNViewComponentView::has_focus_references() const {
	return preferred_focus_pending || names_focus_neighbors(*props);
}

void RNViewComponentView::resolve_focus_references(const std::function<Control *(facebook::react::Tag p_tag)> &p_find_focus_control) {
	const auto path_to = [&](const std::optional<int> &p_tag) {
		Control *target = p_tag ? p_find_focus_control(*p_tag) : nullptr;
		return target && view->is_inside_tree() && target->is_inside_tree() ? view->get_path_to(target) : NodePath();
	};
	view->set_focus_neighbor(SIDE_TOP, path_to(props->nextFocusUp));
	view->set_focus_neighbor(SIDE_BOTTOM, path_to(props->nextFocusDown));
	view->set_focus_neighbor(SIDE_LEFT, path_to(props->nextFocusLeft));
	view->set_focus_neighbor(SIDE_RIGHT, path_to(props->nextFocusRight));
	view->set_focus_next(path_to(props->nextFocusForward));
	if (preferred_focus_pending && view->is_visible_in_tree() && view->get_focus_mode() != Control::FOCUS_NONE) {
		preferred_focus_pending = false;
		view->call_deferred("grab_focus");
	}
}

void RNViewComponentView::update_layout(const facebook::react::LayoutMetrics &p_metrics) {
	RNComponentView::update_layout(p_metrics);
	if (props) {
		_apply_style();
	}
}

void RNViewComponentView::update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	auto emitter = std::static_pointer_cast<const facebook::react::ViewEventEmitter>(p_emitter);
	if (!emitter) {
		view->set_menu_callbacks(nullptr, nullptr);
		view->set_focus_callback(nullptr);
		view->set_accessibility_callbacks(nullptr, nullptr);
		view->set_drag_callbacks(nullptr, nullptr);
		if (effect) {
			effect->get_painter()->set_drag_callbacks(nullptr, nullptr);
		}
		return;
	}
	const auto emit_drop = [emitter, control = view](const Variant &p_data, const Point2 &p_point) {
		const Point2 page = control->get_global_transform_with_canvas().xform(p_point);
		emitter->onGodotDrop(rn_dynamic_from_variant(p_data), p_point.x, p_point.y, page.x, page.y);
	};
	const auto emit_drag_end = [emitter, control = view](bool p_successful) {
		const Point2 page = control->get_global_mouse_position();
		emitter->onGodotDragEnd(p_successful, page.x, page.y);
	};
	view->set_drag_callbacks(emit_drop, emit_drag_end);
	if (effect) {
		effect->get_painter()->set_drag_callbacks(emit_drop, emit_drag_end);
	}
	view->set_accessibility_callbacks([emitter]() { emitter->onKeyboardClick(); }, [emitter](const String &p_name) { emitter->onAccessibilityAction(p_name.utf8().get_data()); });
	view->set_menu_callbacks([emitter](const String &p_id) { emitter->onGodotContextMenuAction(p_id.utf8().get_data()); }, nullptr);
	view->set_focus_callback([emitter](bool p_focused) {
		if (p_focused) {
			emitter->onFocus();
		} else {
			emitter->onBlur();
		}
	});
}

void RNViewComponentView::handle_command(const std::string &p_name, const folly::dynamic &) {
	if (p_name == "focus") {
		view->grab_focus();
	} else if (p_name == "blur") {
		view->release_focus();
	}
}
