#pragma once

#include "../fabric/rn_native_event.h"
#include "../interop/rn_error.h"
#include "../interop/rn_schema.h"

#include "core/object/object_id.h"
#include "core/templates/hash_map.h"
#include "core/variant/dictionary.h"
#include "scene/gui/control.h"

#include <functional>
#include <memory>

class ReactNativeRootView;
class RNShadowNode;

struct RNHostTraits {
	bool creates_host = true;
	bool participates_in_layout = true;
	bool container = true;
	bool measured_leaf = false;
	bool contributes_text = false;
	bool collects_text = false;
	bool focus_target = false;
	bool input_target = false;
	bool has_native_children = true;
	bool emits_layout = true;
	bool accessibility_pending = true;
};

enum class RNMeasureMode {
	UNDEFINED,
	EXACTLY,
	AT_MOST,
};

struct RNMeasureConstraints {
	float width = 0;
	float height = 0;
	RNMeasureMode width_mode = RNMeasureMode::UNDEFINED;
	RNMeasureMode height_mode = RNMeasureMode::UNDEFINED;
};

enum class RNChildLayoutPolicy {
	ORDINARY,
	MEASURED_LEAF,
	SCROLL_HORIZONTAL,
	SCROLL_VERTICAL,
	PRESENTATION,
};

struct RNComponentData {
	virtual ~RNComponentData() = default;
};

struct RNHostGeometry {
	Transform2D transform;
	Transform2D screen_transform;
	Rect2 bounds;
	Rect2 viewport;
	ObjectID viewport_id;
	Point2 scroll_offset;
	Size2 content_size;
	HashMap<int, Vector<Rect2>> span_bounds;
	bool clips_contents = false;
};

struct RNHostEventSink {
	std::function<void(int, const StringName &, const Dictionary &, uint64_t)> emit;
	std::function<void()> invalidate_geometry;
	std::function<void()> invalidate_layout;
	std::function<void()> cancel_input;
	std::function<bool()> is_current;
};

struct RNPreparedHostState {
	Dictionary props;
	String text;
	HashMap<String, uint64_t> declarative_prop_revisions;
	bool branch_targetable = true;
	bool layout_rtl = false;
	std::shared_ptr<const RNComponentData> component_data;
	uint64_t dependency_revision = 0;
};

struct RNHostContext {
	ReactNativeRootView *owner = nullptr;
	ObjectID host_id;
	uint64_t generation = 0;
	int root_tag = 0;
	uint64_t surface_epoch = 0;
	uint64_t revision = 0;
	int tag = 0;
	Size2 presentation_size;
	std::shared_ptr<const RNHostEventSink> event_sink;
};

class RNHostDescriptor {
	StringName name;
	RNHostTraits traits;
	Dictionary view_config;

public:
	RNHostDescriptor(const StringName &p_name, const RNHostTraits &p_traits, const Dictionary &p_view_config = Dictionary());
	virtual ~RNHostDescriptor() = default;

	const StringName &get_name() const { return name; }
	const RNHostTraits &get_traits() const { return traits; }
	const Dictionary &get_view_config() const { return view_config; }

	virtual bool prepare(const RNShadowNode &p_node, RNPreparedHostState &r_state, RNError &r_error) const;
	virtual bool resolve_resources(RNPreparedHostState &r_state, const RNHostContext &p_context, RNError &r_error) const;
	virtual Control *create_host(const RNHostContext &p_context) const;
	virtual bool apply(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &p_context, RNError &r_error) const;
	virtual Variant capture_state(Control *p_host) const;
	virtual void restore_state(Control *p_host, const Variant &p_state) const;
	virtual Size2 measure(const RNPreparedHostState &p_state, const RNMeasureConstraints &p_constraints) const;
	virtual float baseline(const RNPreparedHostState &p_state, const Size2 &p_size) const;
	virtual Control *get_child_container(Control *p_host, const RNHostContext &p_context) const;
	virtual RNChildLayoutPolicy get_child_layout_policy(const RNPreparedHostState &p_state) const;
	virtual Size2 presentation_size(const RNPreparedHostState &p_state, const Size2 &p_inherited) const;
	virtual Rect2 get_child_layout_viewport(const RNPreparedHostState &p_state, const Size2 &p_size) const;
	virtual void after_publish(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &p_context) const;
	virtual RNHostGeometry read_geometry(Control *p_host, const RNHostContext &p_context) const;
	virtual Control *focus_control(Control *p_host) const;
	virtual bool owns_native_activation(const RNPreparedHostState &p_state) const;
	virtual bool owns_input_control(Control *p_host, Control *p_control) const;
	virtual void attach_signals(Control *p_host, const RNHostContext &p_context) const;
	virtual void detach_signals(Control *p_host, const RNHostContext &p_context) const;
	virtual void dispose_state(Control *p_host, const RNHostContext &p_context) const;
	virtual bool dispatch_command(Control *p_host, const StringName &p_command, const Variant &p_arguments, const RNHostContext &p_context, RNError &r_error) const;
	virtual bool dispatch_accessibility_action(Control *p_host, const StringName &p_action, const Variant &p_arguments, const RNHostContext &p_context, RNError &r_error) const;
};
