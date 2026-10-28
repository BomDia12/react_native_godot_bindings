#pragma once

#include "rn_component_view.h"

#include <react/renderer/mounting/ShadowViewMutation.h>

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using RNComponentViewFactory = std::function<std::unique_ptr<RNComponentView>(const facebook::react::ShadowView &)>;

// Component names mapped to the factories that create their Godot views.
// Names without a factory use the fallback.
class RNComponentViewFactories {
	std::unordered_map<std::string, RNComponentViewFactory> factories;
	RNComponentViewFactory fallback;

public:
	void add(const std::string &p_component_name, RNComponentViewFactory p_factory) { factories[p_component_name] = std::move(p_factory); }
	void set_fallback(RNComponentViewFactory p_factory) { fallback = std::move(p_factory); }
	bool has(const std::string &p_component_name) const { return factories.contains(p_component_name); }
	const RNComponentViewFactory *find(const std::string &p_component_name) const;
	const RNComponentViewFactory &get_fallback() const { return fallback; }
};

// Maps one surface's React tags to their component views and applies mounting
// mutations in order.
class RNComponentViewRegistry {
	std::shared_ptr<const RNComponentViewFactories> factories;
	std::unordered_map<facebook::react::Tag, std::unique_ptr<RNComponentView>> views;
	std::unordered_map<facebook::react::Tag, RNComponentView *> roots;
	std::unordered_set<std::string> reported_unknown;
	std::vector<std::pair<facebook::react::Tag, facebook::react::Tag>> removed;
	std::unordered_set<facebook::react::Tag> focus_references;
	std::function<void(RNComponentView *)> change_listener;
	std::function<void(facebook::react::Tag)> destroy_listener;

	void _track_focus_references(RNComponentView *p_view);
	void _resolve_focus_references();

public:
	explicit RNComponentViewRegistry(std::shared_ptr<const RNComponentViewFactories> p_factories);

	void register_root(facebook::react::Tag p_tag, RNComponentView *p_root);
	void unregister_root(facebook::react::Tag p_tag);
	RNComponentView *find(facebook::react::Tag p_tag) const;
	RNComponentView *find_for_control(Control *p_control) const;
	RNComponentView *find_by_native_id(const std::string &p_native_id) const;
	void set_change_listener(std::function<void(RNComponentView *)> p_listener) { change_listener = std::move(p_listener); }
	void set_destroy_listener(std::function<void(facebook::react::Tag)> p_listener) { destroy_listener = std::move(p_listener); }

	void create(const facebook::react::ShadowView &p_view);
	void destroy(facebook::react::Tag p_tag);
	void insert(facebook::react::Tag p_parent, facebook::react::Tag p_child, int p_index);
	void remove(facebook::react::Tag p_parent, facebook::react::Tag p_child);
	void update(const facebook::react::ShadowView &p_old, const facebook::react::ShadowView &p_new);
	void apply(const facebook::react::ShadowViewMutationList &p_mutations);
	void clear();
};
