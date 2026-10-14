#include "rn_component_view_registry.h"

using namespace facebook::react;

void RNComponentViewRegistry::register_factory(const std::string &p_component_name, RNComponentViewFactory p_factory) {
	factories[p_component_name] = std::move(p_factory);
}

bool RNComponentViewRegistry::has_component(const std::string &p_component_name) const {
	return factories.contains(p_component_name);
}

void RNComponentViewRegistry::register_root(Tag p_tag, RNComponentView *p_root) {
	roots[p_tag] = p_root;
}

void RNComponentViewRegistry::unregister_root(Tag p_tag) {
	roots.erase(p_tag);
}

RNComponentView *RNComponentViewRegistry::find(Tag p_tag) const {
	if (auto root = roots.find(p_tag); root != roots.end()) {
		return root->second;
	}
	auto view = views.find(p_tag);
	return view == views.end() ? nullptr : view->second.get();
}

void RNComponentViewRegistry::create(const ShadowView &p_view) {
	auto factory = factories.find(p_view.componentName);
	std::unique_ptr<RNComponentView> view;
	if (factory != factories.end()) {
		view = factory->second(p_view);
	} else {
		if (reported_unknown.insert(p_view.componentName).second) {
			ERR_PRINT(vformat("E_UNKNOWN_COMPONENT: no Godot component view is registered for '%s'.", p_view.componentName));
		}
		ERR_FAIL_COND(!fallback);
		view = fallback(p_view);
	}
	view->update_props(nullptr, p_view.props);
	view->update_state(p_view.state);
	view->update_event_emitter(p_view.eventEmitter);
	view->update_layout(p_view.layoutMetrics);
	views[p_view.tag] = std::move(view);
}

void RNComponentViewRegistry::destroy(Tag p_tag) {
	auto view = views.find(p_tag);
	ERR_FAIL_COND_MSG(view == views.end(), vformat("Mounting deleted unknown React tag %d.", p_tag));
	Control *control = view->second->get_control();
	if (control->get_parent()) {
		control->get_parent()->remove_child(control);
	}
	views.erase(view);
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
}
