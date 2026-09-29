#include "rn_tree_differ.h"

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

namespace {

struct IndexedNode {
	Ref<RNShadowNode> node;
	int parent_tag = 0;
	int depth = 0;
};

struct MutationBuckets {
	Vector<RNMountingMutation> updates;
	Vector<RNMountingMutation> removes;
	Vector<RNMountingMutation> deletes;
	Vector<RNMountingMutation> creates;
	Vector<RNMountingMutation> inserts;
	HashSet<int> updated_tags;
};

bool is_host(const Ref<RNShadowNode> &p_node) {
	return p_node.is_valid() && p_node->view_name != "RCTRootView" && p_node->view_name != "RCTRawText";
}

bool props_equal(const Dictionary &p_left, const Dictionary &p_right) {
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

RNMountContext child_context(const Ref<RNShadowNode> &p_node, const RNMountContext &p_context) {
	RNMountContext result = p_context;
	if (p_node->view_name == "RCTRootView") {
		result.native_parent_tag = p_node->tag;
		return result;
	}
	if (p_node->view_name == "RCTRawText") {
		return result;
	}
	result.native_parent_tag = p_node->tag;
	if (p_node->view_name == "RCTText") {
		result.text_host_tag = p_node->tag;
	}
	const String pointer_events = String(p_node->props.get("pointerEvents", "auto")).to_lower();
	result.branch_targetable = p_context.branch_targetable && pointer_events != "none" && pointer_events != "box-only";
	return result;
}

bool index_tree(const Ref<RNShadowNode> &p_root, HashMap<int, IndexedNode> &r_index, String &r_error) {
	struct Pending {
		Ref<RNShadowNode> node;
		int parent_tag;
		int depth;
	};
	Vector<Pending> pending;
	pending.push_back({ p_root, 0, 0 });
	while (!pending.is_empty()) {
		const Pending current = pending[pending.size() - 1];
		pending.remove_at(pending.size() - 1);
		if (current.node.is_null()) {
			continue;
		}
		if (current.depth >= RNShadowNode::MAX_DEPTH) {
			r_error = "shadow tree exceeds RNShadowNode::MAX_DEPTH";
			return false;
		}
		if (!current.node->structurally_valid) {
			r_error = current.node->structural_error.is_empty() ? vformat("tag %d belongs to an invalid partial tree", current.node->tag) : current.node->structural_error;
			return false;
		}
		if (r_index.has(current.node->tag)) {
			r_error = vformat("duplicate shadow tag %d", current.node->tag);
			return false;
		}
		if (current.node->root_tag != p_root->root_tag || current.node->runtime_generation != p_root->runtime_generation || current.node->surface_epoch != p_root->surface_epoch) {
			r_error = vformat("tag %d belongs to another surface", current.node->tag);
			return false;
		}
		r_index[current.node->tag] = { current.node, current.parent_tag, current.depth };
		HashSet<int> direct_tags;
		for (int i = current.node->children.size() - 1; i >= 0; --i) {
			const Ref<RNShadowNode> &child = current.node->children[i];
			if (child.is_null()) {
				continue;
			}
			if (direct_tags.has(child->tag)) {
				r_error = vformat("tag %d is repeated under parent %d", child->tag, current.node->tag);
				return false;
			}
			direct_tags.insert(child->tag);
			pending.push_back({ child, current.node->tag, current.depth + 1 });
		}
	}
	return true;
}

Vector<Ref<RNShadowNode>> native_children(const Ref<RNShadowNode> &p_node) {
	Vector<Ref<RNShadowNode>> result;
	if (p_node.is_null() || p_node->view_name == "RCTRawText") {
		return result;
	}
	for (const Ref<RNShadowNode> &child : p_node->children) {
		if (is_host(child)) {
			result.push_back(child);
		}
	}
	return result;
}

void add_update(const Ref<RNShadowNode> &p_old, const Ref<RNShadowNode> &p_new, int p_parent_tag, MutationBuckets &r_buckets) {
	if (p_new.is_null() || !is_host(p_new) || r_buckets.updated_tags.has(p_new->tag)) {
		return;
	}
	r_buckets.updated_tags.insert(p_new->tag);
	r_buckets.updates.push_back({ RNMutationType::UPDATE, p_parent_tag, -1, p_old, p_new });
}

void create_subtree(const Ref<RNShadowNode> &p_node, int p_parent_tag, int p_index, MutationBuckets &r_buckets) {
	if (!is_host(p_node)) {
		return;
	}
	r_buckets.creates.push_back({ RNMutationType::CREATE, 0, -1, Ref<RNShadowNode>(), p_node });
	r_buckets.inserts.push_back({ RNMutationType::INSERT, p_parent_tag, p_index, Ref<RNShadowNode>(), p_node });
	const Vector<Ref<RNShadowNode>> children = native_children(p_node);
	for (int i = 0; i < children.size(); ++i) {
		create_subtree(children[i], p_node->tag, i, r_buckets);
	}
}

void delete_subtree(const Ref<RNShadowNode> &p_node, int p_parent_tag, int p_index, MutationBuckets &r_buckets) {
	if (!is_host(p_node)) {
		return;
	}
	const Vector<Ref<RNShadowNode>> children = native_children(p_node);
	for (int i = children.size() - 1; i >= 0; --i) {
		delete_subtree(children[i], p_node->tag, i, r_buckets);
	}
	r_buckets.removes.push_back({ RNMutationType::REMOVE, p_parent_tag, p_index, p_node, Ref<RNShadowNode>() });
	r_buckets.deletes.push_back({ RNMutationType::DELETE, 0, -1, p_node, Ref<RNShadowNode>() });
}

void diff_node(const Ref<RNShadowNode> &p_old, const Ref<RNShadowNode> &p_new, const RNMountContext &p_old_context, const RNMountContext &p_new_context, MutationBuckets &r_buckets, RNTreeDifferStats &r_stats);

void diff_children(const Ref<RNShadowNode> &p_old, const Ref<RNShadowNode> &p_new, const RNMountContext &p_old_context, const RNMountContext &p_new_context, MutationBuckets &r_buckets, RNTreeDifferStats &r_stats) {
	const Vector<Ref<RNShadowNode>> old_children = native_children(p_old);
	const Vector<Ref<RNShadowNode>> new_children = native_children(p_new);
	int prefix = 0;
	while (prefix < old_children.size() && prefix < new_children.size() && old_children[prefix]->tag == new_children[prefix]->tag) {
		diff_node(old_children[prefix], new_children[prefix], p_old_context, p_new_context, r_buckets, r_stats);
		prefix++;
	}
	int old_end = old_children.size();
	int new_end = new_children.size();
	while (old_end > prefix && new_end > prefix && old_children[old_end - 1]->tag == new_children[new_end - 1]->tag) {
		old_end--;
		new_end--;
	}

	HashMap<int, Ref<RNShadowNode>> new_remaining;
	HashMap<int, Ref<RNShadowNode>> old_remaining;
	if (old_end > prefix || new_end > prefix) {
		r_stats.child_maps_allocated++;
	}
	for (int i = prefix; i < new_end; ++i) {
		new_remaining[new_children[i]->tag] = new_children[i];
	}
	for (int i = prefix; i < old_end; ++i) {
		old_remaining[old_children[i]->tag] = old_children[i];
	}
	HashSet<int> inserted;
	int old_index = prefix;
	int new_index = prefix;
	while (old_index < old_end || new_index < new_end) {
		if (old_index < old_end && new_index < new_end && old_children[old_index]->tag == new_children[new_index]->tag) {
			diff_node(old_children[old_index], new_children[new_index], p_old_context, p_new_context, r_buckets, r_stats);
			old_index++;
			new_index++;
			continue;
		}
		if (old_index < old_end && inserted.has(old_children[old_index]->tag)) {
			const Ref<RNShadowNode> next = *new_remaining.getptr(old_children[old_index]->tag);
			r_buckets.removes.push_back({ RNMutationType::REMOVE, p_old->tag, old_index, old_children[old_index], Ref<RNShadowNode>() });
			diff_node(old_children[old_index], next, p_old_context, p_new_context, r_buckets, r_stats);
			old_index++;
			continue;
		}
		if (old_index < old_end && !new_remaining.has(old_children[old_index]->tag)) {
			delete_subtree(old_children[old_index], p_old->tag, old_index, r_buckets);
			old_index++;
			continue;
		}
		if (new_index < new_end) {
			const Ref<RNShadowNode> next = new_children[new_index];
			if (old_remaining.has(next->tag)) {
				r_buckets.inserts.push_back({ RNMutationType::INSERT, p_new->tag, new_index, Ref<RNShadowNode>(), next });
				inserted.insert(next->tag);
			} else {
				create_subtree(next, p_new->tag, new_index, r_buckets);
			}
			new_index++;
			continue;
		}
		break;
	}
	for (int i = 0; i < old_children.size() - old_end; ++i) {
		const Ref<RNShadowNode> old_child = old_children[old_end + i];
		const Ref<RNShadowNode> new_child = new_children[new_end + i];
		diff_node(old_child, new_child, p_old_context, p_new_context, r_buckets, r_stats);
	}
}

void diff_raw_children(const Ref<RNShadowNode> &p_old, const Ref<RNShadowNode> &p_new, const RNMountContext &p_context, MutationBuckets &r_buckets) {
	if (p_context.text_host_tag == 0) {
		return;
	}
	const String old_text = p_old->collect_text();
	const String new_text = p_new->collect_text();
	if (old_text == new_text) {
		return;
	}
	Ref<RNShadowNode> old_host = p_old;
	Ref<RNShadowNode> new_host = p_new;
	if (p_new->tag != p_context.text_host_tag) {
		old_host.unref();
		new_host.unref();
	}
	add_update(old_host, new_host, p_context.native_parent_tag, r_buckets);
}

void diff_node(const Ref<RNShadowNode> &p_old, const Ref<RNShadowNode> &p_new, const RNMountContext &p_old_context, const RNMountContext &p_new_context, MutationBuckets &r_buckets, RNTreeDifferStats &r_stats) {
	if (p_old.is_null() || p_new.is_null()) {
		return;
	}
	if (p_old.ptr() == p_new.ptr() && p_old_context == p_new_context) {
		return;
	}
	r_stats.visited_nodes++;
	const bool context_changed = p_old_context != p_new_context;
	if (is_host(p_new) && (!props_equal(p_old->props, p_new->props) || context_changed || p_old->event_target != p_new->event_target)) {
		add_update(p_old, p_new, p_new_context.native_parent_tag, r_buckets);
	}
	RNMountContext old_children_context = child_context(p_old, p_old_context);
	RNMountContext new_children_context = child_context(p_new, p_new_context);
	if (p_new->view_name == "RCTText" && p_old->collect_text() != p_new->collect_text()) {
		add_update(p_old, p_new, p_new_context.native_parent_tag, r_buckets);
	}
	diff_raw_children(p_old, p_new, new_children_context, r_buckets);
	diff_children(p_old, p_new, old_children_context, new_children_context, r_buckets, r_stats);
}

int node_depth(const Ref<RNShadowNode> &p_node, const HashMap<int, IndexedNode> &p_index) {
	const IndexedNode *entry = p_node.is_valid() ? p_index.getptr(p_node->tag) : nullptr;
	return entry ? entry->depth : 0;
}

template <typename Comparator>
void sort_mutations(Vector<RNMountingMutation> &r_mutations, Comparator p_compare) {
	for (int i = 1; i < r_mutations.size(); ++i) {
		int current = i;
		while (current > 0 && p_compare(r_mutations[current], r_mutations[current - 1])) {
			std::swap(r_mutations.write[current], r_mutations.write[current - 1]);
			current--;
		}
	}
}

} // namespace

bool RNTreeDiffer::diff(const Ref<RNShadowNode> &p_old_root, const Ref<RNShadowNode> &p_new_root, Vector<RNMountingMutation> &r_mutations, String &r_error, RNTreeDifferStats *r_stats) {
	r_mutations.clear();
	r_error = String();
	RNTreeDifferStats stats;
	if (p_new_root.is_null()) {
		r_error = "desired root is null";
		return false;
	}
	if (p_old_root.ptr() == p_new_root.ptr()) {
		if (r_stats) {
			*r_stats = stats;
		}
		return true;
	}
	HashMap<int, IndexedNode> old_index;
	HashMap<int, IndexedNode> new_index;
	if (p_old_root.is_valid() && !index_tree(p_old_root, old_index, r_error)) {
		return false;
	}
	if (!index_tree(p_new_root, new_index, r_error)) {
		return false;
	}
	for (const KeyValue<int, IndexedNode> &entry : new_index) {
		const IndexedNode *old = old_index.getptr(entry.key);
		if (!old) {
			continue;
		}
		if (old->parent_tag != entry.value.parent_tag) {
			r_error = vformat("retained tag %d changed logical parent from %d to %d", entry.key, old->parent_tag, entry.value.parent_tag);
			return false;
		}
		if (old->node->view_name != entry.value.node->view_name) {
			r_error = vformat("retained tag %d changed host type from %s to %s", entry.key, old->node->view_name, entry.value.node->view_name);
			return false;
		}
	}
	MutationBuckets buckets;
	if (p_old_root.is_null()) {
		const Vector<Ref<RNShadowNode>> children = native_children(p_new_root);
		for (int i = 0; i < children.size(); ++i) {
			create_subtree(children[i], p_new_root->tag, i, buckets);
		}
	} else {
		RNMountContext root_context;
		root_context.native_parent_tag = p_new_root->tag;
		diff_node(p_old_root, p_new_root, root_context, root_context, buckets, stats);
	}
	sort_mutations(buckets.removes, [&](const RNMountingMutation &p_left, const RNMountingMutation &p_right) {
		const int left_depth = node_depth(p_left.old_node, old_index);
		const int right_depth = node_depth(p_right.old_node, old_index);
		return left_depth != right_depth ? left_depth > right_depth : p_left.index > p_right.index;
	});
	sort_mutations(buckets.inserts, [&](const RNMountingMutation &p_left, const RNMountingMutation &p_right) {
		const int left_depth = node_depth(p_left.new_node, new_index);
		const int right_depth = node_depth(p_right.new_node, new_index);
		return left_depth != right_depth ? left_depth < right_depth : p_left.index < p_right.index;
	});
	for (const RNMountingMutation &mutation : buckets.updates) {
		r_mutations.push_back(mutation);
	}
	for (const RNMountingMutation &mutation : buckets.removes) {
		r_mutations.push_back(mutation);
	}
	for (const RNMountingMutation &mutation : buckets.deletes) {
		r_mutations.push_back(mutation);
	}
	for (const RNMountingMutation &mutation : buckets.creates) {
		r_mutations.push_back(mutation);
	}
	for (const RNMountingMutation &mutation : buckets.inserts) {
		r_mutations.push_back(mutation);
	}
	if (r_stats) {
		*r_stats = stats;
	}
	return true;
}
