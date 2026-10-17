#include "rn_component_view_registry.h"

#include "../interop/rn_error.h"

using namespace facebook::react;

const RNComponentViewFactory *RNComponentViewFactories::find(const std::string &p_component_name) const {
	auto factory = factories.find(p_component_name);
	return factory == factories.end() ? nullptr : &factory->second;
}

RNComponentViewRegistry::RNComponentViewRegistry(std::shared_ptr<const RNComponentViewFactories> p_factories) :
		factories(std::move(p_factories)) {
}

void RNComponentViewRegistry::register_root(Tag p_tag, RNComponentView *p_root) {
	roots[p_tag] = p_root;
}

void RNComponentViewRegistry::unregister_root(Tag p_tag) {
	roots.erase(p_tag);
}

RNComponentView *RNComponentViewRegistry::find_for_control(Control *p_control) const {
	for (Node *node = p_control; node; node = node->get_parent()) {
		if (node->has_meta(RNComponentView::TAG_META)) {
			RNComponentView *view = find(int(node->get_meta(RNComponentView::TAG_META)));
			return view && view->get_control() == node && view->owns_input_control(p_control) ? view : nullptr;
		}
	}
	return nullptr;
}

RNComponentView *RNComponentViewRegistry::find(Tag p_tag) const {
	if (auto root = roots.find(p_tag); root != roots.end()) {
		return root->second;
	}
	auto view = views.find(p_tag);
	return view == views.end() ? nullptr : view->second.get();
}

void RNComponentViewRegistry::create(const ShadowView &p_view) {
	const RNComponentViewFactory *factory = factories->find(p_view.componentName);
	if (!factory) {
		if (reported_unknown.insert(p_view.componentName).second) {
			ERR_PRINT(vformat("%s: no Godot component view is registered for '%s'.", RNErrorCode::UNKNOWN_COMPONENT, p_view.componentName));
		}
		ERR_FAIL_COND(!factories->get_fallback());
		factory = &factories->get_fallback();
	}
	std::unique_ptr<RNComponentView> view = (*factory)(p_view);
	// The emitter comes first: props can start work, such as a local image
	// load, that reports events before the next update.
	view->update_event_emitter(p_view.eventEmitter);
	view->update_props(nullptr, p_view.props);
	if (p_view.state) {
		view->update_state(p_view.state);
	}
	view->update_layout(p_view.layoutMetrics);
	RNComponentView *created = view.get();
	views[p_view.tag] = std::move(view);
	if (change_listener) {
		change_listener(created);
	}
}

void RNComponentViewRegistry::destroy(Tag p_tag) {
	auto view = views.find(p_tag);
	ERR_FAIL_COND_MSG(view == views.end(), vformat("Mounting deleted unknown React tag %d.", p_tag));
	// Freeing this view's control would free children still waiting to be
	// detached, which may be inserted elsewhere later in the transaction.
	Control *control = view->second->get_control();
	for (const auto &[parent, child] : removed) {
		RNComponentView *pending = find(child);
		if (pending && (parent == p_tag || control->is_ancestor_of(pending->get_control()))) {
			pending->finish_removal();
		}
	}
	views.erase(view);
	if (destroy_listener) {
		destroy_listener(p_tag);
	}
}

void RNComponentViewRegistry::insert(Tag p_parent, Tag p_child, int p_index) {
	RNComponentView *parent = find(p_parent);
	RNComponentView *child = find(p_child);
	ERR_FAIL_COND_MSG(!parent || !child, vformat("Mounting inserted React tag %d into unknown parent %d.", p_child, p_parent));
	parent->insert_child(child, p_index);
}

void RNComponentViewRegistry::remove(Tag p_parent, Tag p_child) {
	RNComponentView *parent = find(p_parent);
	RNComponentView *child = find(p_child);
	ERR_FAIL_COND_MSG(!parent || !child, vformat("Mounting removed React tag %d from unknown parent %d.", p_child, p_parent));
	parent->remove_child(child);
	removed.push_back({ p_parent, p_child });
}

void RNComponentViewRegistry::update(const ShadowView &p_old, const ShadowView &p_new) {
	RNComponentView *view = find(p_new.tag);
	ERR_FAIL_COND_MSG(!view, vformat("Mounting updated unknown React tag %d.", p_new.tag));
	if (p_old.props != p_new.props) {
		view->update_props(p_old.props, p_new.props);
	}
	if (p_old.state != p_new.state) {
		view->update_state(p_new.state);
	}
	if (p_old.eventEmitter != p_new.eventEmitter) {
		view->update_event_emitter(p_new.eventEmitter);
	}
	if (p_old.layoutMetrics != p_new.layoutMetrics) {
		view->update_layout(p_new.layoutMetrics);
	}
	if (change_listener) {
		change_listener(view);
	}
}

void RNComponentViewRegistry::apply(const ShadowViewMutationList &p_mutations) {
	for (const ShadowViewMutation &mutation : p_mutations) {
		switch (mutation.type) {
			case ShadowViewMutation::Create:
				create(mutation.newChildShadowView);
				break;
			case ShadowViewMutation::Delete:
				destroy(mutation.oldChildShadowView.tag);
				break;
			case ShadowViewMutation::Insert:
				insert(mutation.parentTag, mutation.newChildShadowView.tag, mutation.index);
				break;
			case ShadowViewMutation::Remove:
				remove(mutation.parentTag, mutation.oldChildShadowView.tag);
				break;
			case ShadowViewMutation::Update:
				update(mutation.oldChildShadowView, mutation.newChildShadowView);
				break;
			default:
				break;
		}
	}
	for (const auto &[parent, tag] : removed) {
		if (RNComponentView *view = find(tag)) {
			view->finish_removal();
		}
	}
	removed.clear();
}

void RNComponentViewRegistry::clear() {
	views.clear();
	roots.clear();
}
