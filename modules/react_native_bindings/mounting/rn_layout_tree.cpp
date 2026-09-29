#include "rn_layout_tree.h"

#include "../fabric/rn_layout.h"

#include "scene/resources/font.h"
#include "scene/theme/theme_db.h"

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
	return p_node.is_valid() && p_node->view_name != "RCTRawText";
}

YGSize measure_text(YGNodeConstRef p_node, float p_width, YGMeasureMode p_width_mode, float p_height, YGMeasureMode p_height_mode) {
	(void)p_height;
	(void)p_height_mode;
	const RNLayoutMeasureContext *context = static_cast<const RNLayoutMeasureContext *>(YGNodeGetContext(p_node));
	const Ref<Font> font = ThemeDB::get_singleton()->get_fallback_font();
	if (!context || font.is_null()) {
		return YGSize{ 0.0f, 0.0f };
	}
	const float font_size = RNLayout::text_font_size(context->props);
	const float wrap_width = p_width_mode == YGMeasureModeUndefined || !std::isfinite(p_width) ? -1.0f : p_width;
	const Size2 measured = font->get_multiline_string_size(context->text, HORIZONTAL_ALIGNMENT_LEFT, wrap_width, font_size);
	float width = float(measured.width);
	if (p_width_mode == YGMeasureModeExactly) {
		width = p_width;
	} else if (p_width_mode == YGMeasureModeAtMost) {
		width = MIN(width, p_width);
	}
	return YGSize{ width, float(measured.height) };
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

YGNodeRef RNLayoutTree::prepare_node(const Ref<RNShadowNode> &p_node, int p_parent_tag, HashMap<int, bool> &r_seen, String &r_error) {
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
	if (p_node->view_name == "RCTText") {
		const String text = p_node->collect_text();
		if (!record.context->measure_initialized) {
			record.context->text = text;
			record.context->props = p_node->props.duplicate(true);
			record.context->measure_initialized = true;
			YGNodeSetMeasureFunc(record.yoga_node, measure_text);
		} else if (record.context->text != text || props_changed) {
			record.context->text = text;
			record.context->props = p_node->props.duplicate(true);
			YGNodeMarkDirty(record.yoga_node);
			stats.text_nodes_dirtied++;
		}
		return record.yoga_node;
	}

	Vector<YGNodeRef> desired_children;
	for (const Ref<RNShadowNode> &child : p_node->children) {
		if (!is_layout_participant(child)) {
			continue;
		}
		YGNodeRef yoga_child = prepare_node(child, p_node->tag, r_seen, r_error);
		if (!yoga_child) {
			return nullptr;
		}
		desired_children.push_back(yoga_child);
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

bool RNLayoutTree::prepare(const Ref<RNShadowNode> &p_root, const Size2 &p_constraint, HashMap<int, Rect2> &r_layouts, String &r_error) {
	r_error = String();
	if (p_root.is_null()) {
		r_error = "layout root is null";
		return false;
	}
	if (published_root.ptr() == p_root.ptr() && published_constraint.is_equal_approx(p_constraint)) {
		r_layouts = layouts;
		prepared_root = published_root;
		prepared_constraint = published_constraint;
		return true;
	}
	HashMap<int, bool> seen;
	YGNodeRef yoga_root = prepare_node(p_root, 0, seen, r_error);
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
	YGNodeCalculateLayout(yoga_root, float(p_constraint.x), float(p_constraint.y), YGDirectionLTR);
	stats.calculations++;
	r_layouts.clear();
	capture_layout(yoga_root, r_layouts);
	prepared_root = p_root;
	prepared_constraint = p_constraint;
	return true;
}

void RNLayoutTree::publish() {
	published_root = prepared_root;
	published_constraint = prepared_constraint;
	layouts.clear();
	YGNodeRef root = nullptr;
	auto found = published_root.is_valid() ? records.find(published_root->tag) : records.end();
	if (found != records.end()) {
		root = found->second.yoga_node;
	}
	capture_layout(root, layouts);
	free_prepared_removed();
}

bool RNLayoutTree::rebuild(const Ref<RNShadowNode> &p_root, const Size2 &p_constraint, String &r_error) {
	free_all();
	published_root.unref();
	prepared_root.unref();
	layouts.clear();
	HashMap<int, Rect2> rebuilt;
	if (p_root.is_null()) {
		return true;
	}
	if (!prepare(p_root, p_constraint, rebuilt, r_error)) {
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
}
