#pragma once

// SCAFFOLDING. This host object proves the JSI -> Godot path in isolation, with
// hand-written JS and no React. It is deleted in Phase 3, when the real
// nativeFabricUIManager replaces it. Do not build anything on top of it.

#include "core/object/object_id.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"

#include <jsi/jsi.h>

class RNScratchHost : public facebook::jsi::HostObject {
	struct ScratchNode {
		String view_name;
		String text;
		Dictionary props;
		Vector<int> children;
	};

	ObjectID root_view_id;
	HashMap<int, ScratchNode> nodes;

	Dictionary build_tree(int p_tag) const;
	class ReactNativeRootView *get_root_view() const;

	facebook::jsi::Value create_node(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value create_raw_text(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value append_child(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value commit_root(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);

public:
	// The JS shim that exposes this host object's methods as the bare globals the
	// scratch scripts call. Evaluated after the host object is installed.
	static const char *JS_SHIM;
	static const char *GLOBAL_NAME;

	explicit RNScratchHost(ObjectID p_root_view_id);

	// Drops the shadow nodes of the previous bundle. Called on reload.
	void reset_state();

	facebook::jsi::Value get(facebook::jsi::Runtime &rt, const facebook::jsi::PropNameID &name) override;
	std::vector<facebook::jsi::PropNameID> getPropertyNames(facebook::jsi::Runtime &rt) override;
};
