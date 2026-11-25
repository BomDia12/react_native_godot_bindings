#include "rn_viewport_component_view.h"

#include "../host/react_native_host.h"

#include "core/object/callable_mp.h"
#include "scene/main/viewport.h"

#include <react/renderer/components/ReactNativeGodotSpec/EventEmitters.h>
#include <react/renderer/components/ReactNativeGodotSpec/Props.h>

using namespace facebook::react;

namespace {

TextureRect::StretchMode stretch_mode(GodotViewportViewResizeMode p_mode) {
	switch (p_mode) {
		case GodotViewportViewResizeMode::Contain:
			return TextureRect::STRETCH_KEEP_ASPECT_CENTERED;
		case GodotViewportViewResizeMode::Stretch:
			return TextureRect::STRETCH_SCALE;
		case GodotViewportViewResizeMode::Center:
			return TextureRect::STRETCH_KEEP_CENTERED;
		case GodotViewportViewResizeMode::Repeat:
			return TextureRect::STRETCH_TILE;
		default:
			return TextureRect::STRETCH_KEEP_ASPECT_COVERED;
	}
}

} // namespace

void RNViewportViewControl::set_callbacks(std::function<void()> p_loaded, std::function<void(const String &)> p_failed) {
	loaded = std::move(p_loaded);
	failed = std::move(p_failed);
}

SubViewport *RNViewportViewControl::_viewport() const {
	return Object::cast_to<SubViewport>(ObjectDB::get_instance(shown));
}

void RNViewportViewControl::set_viewport_name(const String &p_name) {
	if (viewport_name != p_name) {
		viewport_name = p_name;
		_resolve();
	}
}

void RNViewportViewControl::set_match_size(bool p_match) {
	match_size = p_match;
	_update_viewport_size();
}

void RNViewportViewControl::set_forward_input(bool p_forward) {
	forward_input = p_forward;
	set_focus_mode(forward_input ? FOCUS_CLICK : FOCUS_NONE);
}

// The connections stay while a viewport is out of the tree, so the view shows it
// again when it comes back, and they end when its name means another viewport.
void RNViewportViewControl::_follow_viewport(SubViewport *p_viewport) {
	const ObjectID next = p_viewport ? p_viewport->get_instance_id() : ObjectID();
	if (next == followed) {
		return;
	}
	const Callable exiting = callable_mp(this, &RNViewportViewControl::_viewport_exiting);
	const Callable entered = callable_mp(this, &RNViewportViewControl::_resolve);
	if (SubViewport *previous = Object::cast_to<SubViewport>(ObjectDB::get_instance(followed))) {
		previous->disconnect(SceneStringName(tree_exiting), exiting);
		previous->disconnect(SceneStringName(tree_entered), entered);
	}
	followed = next;
	if (p_viewport) {
		p_viewport->connect(SceneStringName(tree_exiting), exiting);
		p_viewport->connect(SceneStringName(tree_entered), entered);
	}
}

void RNViewportViewControl::_resolve() {
	ReactNativeHost *host = ReactNativeHost::get_singleton();
	SubViewport *next = host ? host->find_viewport(viewport_name) : nullptr;
	SubViewport *shown_next = next && next->is_inside_tree() ? next : nullptr;
	_follow_viewport(next);
	if (resolved && (shown_next ? shown_next->get_instance_id() : ObjectID()) == shown) {
		return;
	}
	resolved = true;
	shown = shown_next ? shown_next->get_instance_id() : ObjectID();
	set_texture(shown_next ? Ref<Texture2D>(shown_next->get_texture()) : Ref<Texture2D>());
	if (shown_next) {
		_update_viewport_size();
		if (loaded) {
			loaded();
		}
	} else if (failed && !viewport_name.is_empty()) {
		failed(next ? vformat("The viewport registered as '%s' isn't in the scene tree.", viewport_name) : vformat("No viewport is registered as '%s'.", viewport_name));
	}
}

void RNViewportViewControl::_registration_changed(const String &p_name) {
	if (p_name == viewport_name) {
		_resolve();
	}
}

void RNViewportViewControl::_viewport_exiting() {
	shown = ObjectID();
	set_texture(Ref<Texture2D>());
	if (failed) {
		failed(vformat("The viewport registered as '%s' left the scene tree.", viewport_name));
	}
}

void RNViewportViewControl::_update_viewport_size() {
	SubViewport *viewport = _viewport();
	if (viewport && match_size && is_inside_tree()) {
		const real_t scale = Math::abs(get_viewport()->get_final_transform().get_scale().x);
		viewport->set_size((get_size() * scale).ceil());
	}
}

// From this control's coordinates to the viewport's, through the stretch mode.
Transform2D RNViewportViewControl::_to_viewport() const {
	const SubViewport *viewport = _viewport();
	const Size2 source = viewport ? Size2(viewport->get_size()) : Size2();
	const Size2 own = get_size();
	if (source.x <= 0 || source.y <= 0 || own.x <= 0 || own.y <= 0) {
		return Transform2D();
	}
	Size2 scale = own / source;
	switch (get_stretch_mode()) {
		case STRETCH_KEEP_ASPECT_CENTERED:
			scale = Size2(1, 1) * MIN(scale.x, scale.y);
			break;
		case STRETCH_KEEP_ASPECT_COVERED:
			scale = Size2(1, 1) * MAX(scale.x, scale.y);
			break;
		case STRETCH_KEEP_CENTERED:
		case STRETCH_TILE:
			scale = Size2(1, 1);
			break;
		default:
			break;
	}
	const Point2 origin = get_stretch_mode() == STRETCH_TILE || get_stretch_mode() == STRETCH_SCALE ? Point2() : (own - source * scale) / 2;
	return Transform2D(0, scale, 0, origin).affine_inverse();
}

void RNViewportViewControl::gui_input(const Ref<InputEvent> &p_event) {
	TextureRect::gui_input(p_event);
	SubViewport *viewport = _viewport();
	if (forward_input && viewport) {
		viewport->push_input(p_event->xformed_by(_to_viewport()));
	}
}

void RNViewportViewControl::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (ReactNativeHost *host = ReactNativeHost::get_singleton()) {
				host->connect("viewport_changed", callable_mp(this, &RNViewportViewControl::_registration_changed));
			}
			_resolve();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (ReactNativeHost *host = ReactNativeHost::get_singleton()) {
				host->disconnect("viewport_changed", callable_mp(this, &RNViewportViewControl::_registration_changed));
			}
		} break;
		case NOTIFICATION_RESIZED: {
			_update_viewport_size();
		} break;
	}
}

RNViewportViewComponentView::RNViewportViewComponentView(Tag p_tag) :
		RNComponentView(p_tag, memnew(RNViewportViewControl)), view(Object::cast_to<RNViewportViewControl>(get_control())) {
	view->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
}

RNViewportViewComponentView::~RNViewportViewComponentView() {
	if (has_control()) {
		view->set_callbacks(nullptr, nullptr);
	}
}

void RNViewportViewComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto &next = static_cast<const GodotViewportViewProps &>(*p_new);
	view->set_stretch_mode(stretch_mode(next.resizeMode));
	view->set_match_size(next.matchSize);
	view->set_forward_input(next.forwardInput);
	view->set_viewport_name(String::utf8(next.viewport.c_str()));
}

void RNViewportViewComponentView::update_event_emitter(const EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	auto emitter = std::static_pointer_cast<const GodotViewportViewEventEmitter>(p_emitter);
	if (!emitter) {
		view->set_callbacks(nullptr, nullptr);
		return;
	}
	view->set_callbacks([emitter]() { emitter->onLoad({}); }, [emitter](const String &p_message) { emitter->onError({ .message = p_message.utf8().get_data() }); });
}
