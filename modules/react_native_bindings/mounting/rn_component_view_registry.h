#pragma once

#include "rn_component_view.h"

#include <react/renderer/mounting/ShadowViewMutation.h>

#include <functional>
#include <unordered_map>
#include <unordered_set>

using RNComponentViewFactory = std::function<std::unique_ptr<RNComponentView>(const facebook::react::ShadowView &)>;

// Maps React tags to their component views and component names to the
// factories that create them, and applies mounting mutations in order.
class RNComponentViewRegistry {
	std::unordered_map<std::string, RNComponentViewFactory> factories;
	RNComponentViewFactory fallback;
	std::unordered_map<facebook::react::Tag, std::unique_ptr<RNComponentView>> views;
	std::unordered_map<facebook::react::Tag, RNComponentView *> roots;
	std::unordered_set<std::string> reported_unknown;

public:
	void register_factory(const std::string &p_component_name, RNComponentViewFactory p_factory);
	void set_fallback_factory(RNComponentViewFactory p_factory) { fallback = std::move(p_factory); }
	bool has_component(const std::string &p_component_name) const;

	void register_root(facebook::react::Tag p_tag, RNComponentView *p_root);
	void unregister_root(facebook::react::Tag p_tag);
	RNComponentView *find(facebook::react::Tag p_tag) const;

	void create(const facebook::react::ShadowView &p_view);
	void destroy(facebook::react::Tag p_tag);
	void insert(facebook::react::Tag p_parent, facebook::react::Tag p_child, int p_index);
	void remove(facebook::react::Tag p_parent, facebook::react::Tag p_child);
	void update(const facebook::react::ShadowView &p_old, const facebook::react::ShadowView &p_new);
	void apply(const facebook::react::ShadowViewMutationList &p_mutations);
};
