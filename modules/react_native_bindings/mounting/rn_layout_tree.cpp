#include "rn_layout_tree.h"

#include "../fabric/rn_layout.h"

#include <yoga/Yoga.h>

#include <cmath>
#include <unordered_set>

namespace {

bool dictionaries_equal(const Dictionary &p_left, const Dictionary &p_right) {
	if (p_left.size() != p_right.size()) {
		return false;
	}
	const Array keys = p_left.keys();
	for (int i = 0; i < keys.size(); ++i) {
		const Variant key = keys[i];
		if (!p_right.has(key) || p_left[key] != p_right[key]) {
			return false;
		}
	}
	return true;
}

bool is_layout_participant(const Ref<RNShadowNode> &p_node) {
	return p_node.is_valid() && p_node->descriptor && p_node->descriptor->get_traits().participates_in_layout;
}

RNMeasureMode measure_mode(YGMeasureMode p_mode) {
	switch (p_mode) {
		case YGMeasureModeExactly:
			return RNMeasureMode::EXACTLY;
		case YGMeasureModeAtMost:
			return RNMeasureMode::AT_MOST;
		case YGMeasureModeUndefined:
			return RNMeasureMode::UNDEFINED;
	}
	return RNMeasureMode::UNDEFINED;
}

YGSize measure_descriptor(YGNodeConstRef p_node, float p_width, YGMeasureMode p_width_mode, float p_height, YGMeasureMode p_height_mode) {
	const RNLayoutMeasureContext *context = static_cast<const RNLayoutMeasureContext *>(YGNodeGetContext(p_node));
	if (!context || !context->descriptor) {
		return YGSize{ 0.0f, 0.0f };
	}
	RNMeasureConstraints constraints;
	constraints.width = p_width;
	constraints.height = p_height;
	constraints.width_mode = measure_mode(p_width_mode);
	constraints.height_mode = measure_mode(p_height_mode);
	const Size2 measured = context->descriptor->measure(context->prepared_state, constraints);
	return YGSize{ measured.x, measured.y };
}

float baseline_descriptor(YGNodeConstRef p_node, float p_width, float p_height) {
	const RNLayoutMeasureContext *context = static_cast<const RNLayoutMeasureContext *>(YGNodeGetContext(p_node));
	return context && context->descriptor ? context->descriptor->baseline(context->prepared_state, Size2(p_width, p_height)) : p_height;
}

} // namespace

RNLayoutTree::RNLayoutTree() {
	config = YGConfigNew();
}

RNLayoutTree::~RNLayoutTree() {
	free_all();
	if (config) {
		YGConfigFree(config);
	}
}

void RNLayoutTree::free_all() {
	for (auto &entry : records) {
		if (entry.second.yoga_node) {
			YGNodeRemoveAllChildren(entry.second.yoga_node);
		}
	}
	for (auto &entry : records) {
		if (entry.second.yoga_node) {
			YGNodeFree(entry.second.yoga_node);
			stats.nodes_freed++;
		}
	}
	records.clear();
	free_prepared_removed();
}

void RNLayoutTree::free_prepared_removed() {
	for (Record &record : prepared_removed_records) {
		if (record.yoga_node) {
			YGNodeRemoveAllChildren(record.yoga_node);
		}
	}
	for (Record &record : prepared_removed_records) {
		if (record.yoga_node) {
			YGNodeFree(record.yoga_node);
			stats.nodes_freed++;
		}
	}
	prepared_removed_records.clear();
}

YGNodeRef RNLayoutTree::prepare_node(const Ref<RNShadowNode> &p_node, int p_parent_tag, const HashMap<int, RNPreparedHostState> *p_prepared_states, HashMap<int, bool> &r_seen, String &r_error) {
	if (!is_layout_participant(p_node)) {
		return nullptr;
	}
	r_seen[p_node->tag] = true;
	auto found = records.find(p_node->tag);
	if (found == records.end()) {
		Record record;
		record.yoga_node = YGNodeNewWithConfig(config);
		record.parent_tag = p_parent_tag;
		record.context = std::make_unique<RNLayoutMeasureContext>();
		record.context->tag = p_node->tag;
		YGNodeSetContext(record.yoga_node, record.context.get());
		found = records.emplace(p_node->tag, std::move(record)).first;
		stats.nodes_created++;
	} else if (found->second.parent_tag != p_parent_tag) {
		r_error = vformat("retained Yoga tag %d changed parent from %d to %d", p_node->tag, found->second.parent_tag, p_parent_tag);
		return nullptr;
	}
	Record &record = found->second;
	const bool props_changed = !dictionaries_equal(record.props, p_node->props);
	if (props_changed) {
		RNLayout::reset_style(record.yoga_node);
		RNLayout::apply_style(record.yoga_node, p_node->props);
		record.props = p_node->props.duplicate(true);
		stats.style_writes++;
	}
	if (p_node->descriptor->get_traits().measured_leaf) {
		RNPreparedHostState prepared_state;
		if (p_prepared_states) {
			const RNPreparedHostState *state = p_prepared_states->getptr(p_node->tag);
			if (!state) {
				r_error = vformat("missing prepared descriptor state for measured tag %d", p_node->tag);
				return nullptr;
			}
			prepared_state = *state;
		} else {
			RNError prepare_error;
			if (!p_node->descriptor->prepare(*p_node.ptr(), prepared_state, prepare_error) || !p_node->descriptor->resolve_resources(prepared_state, RNHostContext(), prepare_error)) {
				r_error = prepare_error.describe();
				return nullptr;
			}
		}
		const String text = prepared_state.text;
		if (!record.context->measure_initialized) {
			record.context->text = text;
			record.context->props = p_node->props.duplicate(true);
			record.context->prepared_state = prepared_state;
			record.context->descriptor = p_node->descriptor;
			record.context->measure_initialized = true;
			YGNodeSetMeasureFunc(record.yoga_node, measure_descriptor);
			YGNodeSetBaselineFunc(record.yoga_node, baseline_descriptor);
		} else if (record.context->text != text || props_changed || record.context->prepared_state.dependency_revision != prepared_state.dependency_revision) {
			record.context->text = text;
			record.context->props = p_node->props.duplicate(true);
			record.context->prepared_state = prepared_state;
			record.context->descriptor = p_node->descriptor;
			YGNodeMarkDirty(record.yoga_node);
			stats.text_nodes_dirtied++;
		}
		return record.yoga_node;
	}

	Boundary boundary;
	boundary.tag = p_node->tag;
	boundary.descriptor = p_node->descriptor;
	if (p_prepared_states && p_prepared_states->has(p_node->tag)) {
		boundary.state = (*p_prepared_states)[p_node->tag];
	} else {
		RNError error;
		if (!p_node->descriptor->prepare(*p_node.ptr(), boundary.state, error) || !p_node->descriptor->resolve_resources(boundary.state, RNHostContext(), error)) {
			r_error = error.describe();
			return nullptr;
		}
	}
	boundary.policy = boundary.descriptor->get_child_layout_policy(boundary.state);
	const bool independent = boundary.policy == RNChildLayoutPolicy::SCROLL_HORIZONTAL || boundary.policy == RNChildLayoutPolicy::SCROLL_VERTICAL || boundary.policy == RNChildLayoutPolicy::PRESENTATION;
	const int boundary_index = independent ? boundaries.size() : -1;
	if (independent) {
		boundaries.push_back(boundary);
	}
	Vector<YGNodeRef> desired_children;
	for (const Ref<RNShadowNode> &child : p_node->children) {
		if (!is_layout_participant(child)) {
			continue;
		}
		YGNodeRef yoga_child = prepare_node(child, p_node->tag, p_prepared_states, r_seen, r_error);
		if (!yoga_child) {
			return nullptr;
		}
		if (independent) {
			if (YGNodeRef owner = YGNodeGetOwner(yoga_child)) {
				YGNodeRemoveChild(owner, yoga_child);
			}
			boundaries.write[boundary_index].children.push_back(yoga_child);
		} else {
			desired_children.push_back(yoga_child);
		}
	}
	bool child_order_changed = YGNodeGetChildCount(record.yoga_node) != size_t(desired_children.size());
	if (!child_order_changed) {
		for (int i = 0; i < desired_children.size(); ++i) {
			if (YGNodeGetChild(record.yoga_node, i) != desired_children[i]) {
				child_order_changed = true;
				break;
			}
		}
	}
	if (child_order_changed) {
		YGNodeRemoveAllChildren(record.yoga_node);
		for (int i = 0; i < desired_children.size(); ++i) {
			YGNodeRef child = desired_children[i];
			if (YGNodeGetOwner(child)) {
				YGNodeRemoveChild(YGNodeGetOwner(child), child);
			}
			YGNodeInsertChild(record.yoga_node, child, i);
		}
	}
	return record.yoga_node;
}

void RNLayoutTree::capture_layout(YGNodeRef p_node, HashMap<int, Rect2> &r_layouts) const {
	if (!p_node) {
		return;
	}
	const RNLayoutMeasureContext *context = static_cast<const RNLayoutMeasureContext *>(YGNodeGetContext(p_node));
	const int tag = context ? context->tag : 0;
	if (tag != 0) {
		r_layouts[tag] = Rect2(Point2(YGNodeLayoutGetLeft(p_node), YGNodeLayoutGetTop(p_node)), Size2(YGNodeLayoutGetWidth(p_node), YGNodeLayoutGetHeight(p_node)));
	}
	for (size_t i = 0; i < YGNodeGetChildCount(p_node); ++i) {
		capture_layout(YGNodeGetChild(p_node, i), r_layouts);
	}
}

bool RNLayoutTree::prepare(const Ref<RNShadowNode> &p_root, const Size2 &p_constraint, HashMap<int, Rect2> &r_layouts, String &r_error, const HashMap<int, RNPreparedHostState> *p_prepared_states) {
	r_error = String();
	if (p_root.is_null()) {
		r_error = "layout root is null";
		return false;
	}
	prepared_dependencies.clear();
	bool dependencies_match = true;
	if (p_prepared_states) {
		for (const KeyValue<int, RNPreparedHostState> &entry : *p_prepared_states) {
			prepared_dependencies[entry.key] = entry.value.dependency_revision;
			const uint64_t *published = published_dependencies.getptr(entry.key);
			dependencies_match = dependencies_match && published && *published == entry.value.dependency_revision;
		}
	}
	dependencies_match = dependencies_match && prepared_dependencies.size() == published_dependencies.size();
	if (published_root.ptr() == p_root.ptr() && published_constraint.is_equal_approx(p_constraint) && dependencies_match) {
		r_layouts = layouts;
		prepared_root = published_root;
		prepared_constraint = published_constraint;
		prepared_layouts = layouts;
		return true;
	}
	boundaries.clear();
	HashMap<int, bool> seen;
	YGNodeRef yoga_root = prepare_node(p_root, 0, p_prepared_states, seen, r_error);
	if (!yoga_root) {
		return false;
	}
	for (auto it = records.begin(); it != records.end();) {
		if (seen.has(it->first)) {
			++it;
			continue;
		}
		YGNodeRef node = it->second.yoga_node;
		if (YGNodeRef owner = YGNodeGetOwner(node)) {
			YGNodeRemoveChild(owner, node);
		}
		prepared_removed_records.push_back(std::move(it->second));
		it = records.erase(it);
	}
	YGNodeCalculateLayout(yoga_root, float(p_constraint.x), float(p_constraint.y), (p_prepared_states && p_prepared_states->has(p_root->tag) ? (*p_prepared_states)[p_root->tag].layout_rtl : String(p_root->props.get("direction", "ltr")) == "rtl") ? YGDirectionRTL : YGDirectionLTR);
	stats.calculations++;
	r_layouts.clear();
	capture_layout(yoga_root, r_layouts);
	for (const Boundary &boundary : boundaries) {
		const Rect2 *outer = r_layouts.getptr(boundary.tag);
		if (!outer) {
			r_error = "Missing layout boundary";
			return false;
		}
		const Rect2 viewport = boundary.descriptor->get_child_layout_viewport(boundary.state, outer->size);
		for (YGNodeRef child : boundary.children) {
			const bool horizontal = boundary.policy == RNChildLayoutPolicy::SCROLL_HORIZONTAL;
			const bool vertical = boundary.policy == RNChildLayoutPolicy::SCROLL_VERTICAL;
			YGNodeCalculateLayout(child, horizontal ? YGUndefined : viewport.size.x, vertical ? YGUndefined : viewport.size.y, boundary.state.layout_rtl ? YGDirectionRTL : YGDirectionLTR);
			capture_layout(child, r_layouts);
			const auto *context = static_cast<const RNLayoutMeasureContext *>(YGNodeGetContext(child));
			if (context) {
				r_layouts[context->tag].position = Point2();
			}
		}
	}
	prepared_layouts = r_layouts;
	prepared_root = p_root;
	prepared_constraint = p_constraint;
	return true;
}

void RNLayoutTree::publish() {
	published_root = prepared_root;
	published_constraint = prepared_constraint;
	published_dependencies = prepared_dependencies;
	layouts = prepared_layouts;
	free_prepared_removed();
}

bool RNLayoutTree::rebuild(const Ref<RNShadowNode> &p_root, const Size2 &p_constraint, String &r_error, const HashMap<int, RNPreparedHostState> *p_prepared_states) {
	free_all();
	published_root.unref();
	prepared_root.unref();
	layouts.clear();
	HashMap<int, Rect2> rebuilt;
	if (p_root.is_null()) {
		return true;
	}
	if (!prepare(p_root, p_constraint, rebuilt, r_error, p_prepared_states)) {
		return false;
	}
	publish();
	return true;
}

void RNLayoutTree::clear() {
	free_all();
	published_root.unref();
	prepared_root.unref();
	layouts.clear();
	published_constraint = Size2();
	prepared_constraint = Size2();
	published_dependencies.clear();
	prepared_dependencies.clear();
}
