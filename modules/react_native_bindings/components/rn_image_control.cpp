#include "rn_image_control.h"

#include "../fabric/rn_shadow_node.h"
#include "../fabric/rn_view_style.h"

#include "core/object/callable_mp.h"
namespace {
Dictionary image_config() {
	Dictionary events;
	for (const char *name : { "LoadStart", "Progress", "Load", "Error", "LoadEnd" }) {
		Dictionary registration;
		registration["registrationName"] = String("on") + name;
		events[String("top") + name] = registration;
	}
	Dictionary config;
	config["directEventTypes"] = events;
	return config;
}
class RNImageDescriptor : public RNHostDescriptor {
public:
	RNImageDescriptor() :
			RNHostDescriptor("RCTImageView", RNHostTraits{ true, true, false, true, false, false, false, true, false, true, true }, image_config()) {}
	bool prepare(const RNShadowNode &p_node, RNPreparedHostState &r_state, RNError &r_error) const override {
		if (!RNHostDescriptor::prepare(p_node, r_state, r_error)) {
			return false;
		}
		auto component = std::make_shared<RNImageData>();
		if (r_state.props.has("source")) {
			if (r_state.props["source"].get_type() != Variant::DICTIONARY || !RNImageService::normalize(r_state.props["source"], component->source, r_error)) {
				if (!r_error.is_set()) {
					r_error = RNError::make(RNErrorCode::VALIDATION, "Image source requires a record", "image.prepare");
				}
				return false;
			}
			const Dictionary source = r_state.props["source"];
			for (const char *key : { "width", "height" }) {
				const double size = source.get(key, 0.0);
				if (!Math::is_finite(size) || size < 0 || size > 16384) {
					r_error = RNError::make(RNErrorCode::VALIDATION, "Invalid image intrinsic size", "image.prepare", key);
					return false;
				}
			}
			component->intrinsic = Size2(source.get("width", 0), source.get("height", 0));
			component->has_source = true;
		}
		const String mode = r_state.props.get("resizeMode", "cover");
		if (mode != "cover" && mode != "contain" && mode != "stretch" && mode != "center" && mode != "repeat") {
			r_error = RNError::make(RNErrorCode::UNSUPPORTED, "Unsupported Image resizeMode", "image.prepare");
			return false;
		}
		r_state.component_data = component;
		return true;
	}
	bool resolve_resources(RNPreparedHostState &r_state, const RNHostContext &p_context, RNError &) const override {
		auto component = std::make_shared<RNImageData>(*std::static_pointer_cast<const RNImageData>(r_state.component_data));
		component->service = RNImageService::for_generation(p_context.generation);
		if (component->has_source) {
			component->resource = component->service->cached(component->source);
			if (component->resource) {
				component->intrinsic = Size2(component->resource->texture->get_size()) / component->source.scale;
			}
		}
		r_state.dependency_revision = component->resource ? uint64_t(component->resource->texture->get_instance_id()) : component->source.key.hash();
		r_state.component_data = component;
		return true;
	}
	Control *create_host(const RNHostContext &) const override { return memnew(RNImageControl); }
	bool apply(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &, RNError &) const override {
		auto *image = Object::cast_to<RNImageControl>(p_host);
		const String mode = p_state.props.get("resizeMode", "cover");
		image->set_stretch_mode(mode == "cover" ? TextureRect::STRETCH_KEEP_ASPECT_COVERED : mode == "contain" ? TextureRect::STRETCH_KEEP_ASPECT_CENTERED
						: mode == "center"																	   ? TextureRect::STRETCH_KEEP_CENTERED
						: mode == "repeat"																	   ? TextureRect::STRETCH_TILE
																											   : TextureRect::STRETCH_SCALE);
		image->set_visible(String(p_state.props.get("display", "flex")) != "none");
		image->set_modulate(Color(1, 1, 1, RNViewStyle::opacity_of(p_state.props)));
		Color tint;
		image->set_self_modulate(RNViewStyle::color_of(p_state.props, "tintColor", tint) ? tint : Color(1, 1, 1));
		image->set_mouse_filter(p_state.branch_targetable ? Control::MOUSE_FILTER_PASS : Control::MOUSE_FILTER_IGNORE);
		return true;
	}
	Variant capture_state(Control *p_host) const override {
		auto *image = Object::cast_to<RNImageControl>(p_host);
		Dictionary state;
		state["mode"] = int(image->get_stretch_mode());
		state["visible"] = image->is_visible();
		state["modulate"] = image->get_modulate();
		state["tint"] = image->get_self_modulate();
		state["texture"] = image->get_texture();
		return state;
	}
	void restore_state(Control *p_host, const Variant &p_state) const override {
		auto *image = Object::cast_to<RNImageControl>(p_host);
		Dictionary state = p_state;
		image->set_stretch_mode(TextureRect::StretchMode(int(state["mode"])));
		image->set_visible(state["visible"]);
		image->set_modulate(state["modulate"]);
		image->set_self_modulate(state["tint"]);
		image->set_texture(state["texture"]);
	}
	Size2 measure(const RNPreparedHostState &p_state, const RNMeasureConstraints &) const override {
		auto component = std::static_pointer_cast<const RNImageData>(p_state.component_data);
		return component->intrinsic;
	}
	void after_publish(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &p_context) const override { Object::cast_to<RNImageControl>(p_host)->publish(p_state, p_context); }
};
} //namespace
RNImageControl::RNImageControl() {
	set_expand_mode(EXPAND_IGNORE_SIZE);
	set_clip_contents(true);
}
RNImageControl::~RNImageControl() {
	_observe_texture(Ref<Texture2D>());
	++source_token;
	if (service && subscription) {
		service->cancel(subscription);
	}
}
void RNImageControl::_emit(const StringName &p_name, const Dictionary &p_payload) {
	if (published_context.event_sink && published_context.event_sink->emit) {
		published_context.event_sink->emit(published_context.tag, p_name, p_payload, published_context.revision);
	}
}
void RNImageControl::publish(const RNPreparedHostState &p_state, const RNHostContext &p_context) {
	const auto component = std::static_pointer_cast<const RNImageData>(p_state.component_data);
	published_context = p_context;
	const String key = component->has_source ? component->source.key : String();
	if (key == source_key && service == component->service) {
		return;
	}
	++source_token;
	if (service && subscription) {
		service->cancel(subscription);
	}
	subscription = 0;
	source_key = key;
	service = component->service;
	if (!component->has_source) {
		_observe_texture(Ref<Texture2D>());
		resource.reset();
		set_texture(Ref<Texture2D>());
		return;
	}
	_emit("topLoadStart");
	const uint64_t token = source_token;
	const ObjectID host_id = get_instance_id();
	const auto request = service->request(component->source, [host_id, token, source = component->source](RNImageResult result) { auto *host = Object::cast_to<RNImageControl>(ObjectDB::get_instance(host_id)); if (host) { host->_loaded(token, source, result); } });
	if (token == source_token) {
		subscription = request;
	}
}
void RNImageControl::_loaded(uint64_t p_token, const RNImageSource &p_source, const RNImageResult &p_result) {
	if (p_token != source_token || (published_context.event_sink && published_context.event_sink->is_current && !published_context.event_sink->is_current())) {
		return;
	}
	subscription = 0;
	if (p_result.error.is_set()) {
		Dictionary event;
		event["error"] = p_result.error.message;
		_emit("topError", event);
	} else {
		resource = p_result.resource;
		set_texture(resource->texture);
		_observe_texture(resource->texture);
		Dictionary source;
		source["uri"] = p_source.uri;
		source["width"] = resource->texture->get_width() / p_source.scale;
		source["height"] = resource->texture->get_height() / p_source.scale;
		Dictionary event;
		event["source"] = source;
		_emit("topLoad", event);
		if (published_context.event_sink && published_context.event_sink->invalidate_layout) {
			published_context.event_sink->invalidate_layout();
		}
	}
	_emit("topLoadEnd");
}
std::shared_ptr<const RNHostDescriptor> rn_image_descriptor() {
	return std::make_shared<RNImageDescriptor>();
}

void RNImageControl::_observe_texture(const Ref<Texture2D> &p_texture) {
	if (observed_texture == p_texture) {
		return;
	}
	if (observed_texture.is_valid()) {
		observed_texture->disconnect("changed", callable_mp(this, &RNImageControl::_source_changed));
	}
	observed_texture = p_texture;
	if (observed_texture.is_valid()) {
		observed_texture->connect("changed", callable_mp(this, &RNImageControl::_source_changed));
	}
}
void RNImageControl::_source_changed() {
	if (published_context.event_sink && published_context.event_sink->invalidate_layout) {
		published_context.event_sink->invalidate_layout();
	}
}
