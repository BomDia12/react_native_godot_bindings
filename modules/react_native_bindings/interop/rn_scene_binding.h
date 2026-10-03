#pragma once

#include "rn_schema.h"

#include "core/io/resource.h"
#include "core/object/object_id.h"
#include "core/templates/hash_map.h"

struct RNSceneCommand {
	StringName method;
	RNMethodSchema schema;
};
struct RNSceneSignal {
	StringName event;
	Vector<StringName> arguments;
	RNValueSchema payload;
};
struct RNSceneAttachment {
	ObjectID target;
	String capability;
	int schema_version = 1;
	StringName snapshot_method;
	RNValueSchema snapshot_schema;
	HashMap<StringName, RNSceneCommand> commands;
	HashMap<StringName, RNSceneSignal> signals;
	uint64_t identity = 0;
};

class RNSceneBinding : public Resource {
	GDCLASS(RNSceneBinding, Resource);

	String capability;
	int schema_version = 1;
	StringName snapshot_method;
	Dictionary snapshot_schema;
	Dictionary commands;
	Dictionary signals;

protected:
	static void _bind_methods();

public:
	void set_capability(const String &p_value) { capability = p_value; }
	String get_capability() const { return capability; }
	void set_schema_version(int p_value) { schema_version = p_value; }
	int get_schema_version() const { return schema_version; }
	void set_snapshot_method(const StringName &p_value) { snapshot_method = p_value; }
	StringName get_snapshot_method() const { return snapshot_method; }
	void set_snapshot_schema(const Dictionary &p_value) { snapshot_schema = p_value; }
	Dictionary get_snapshot_schema() const { return snapshot_schema; }
	void set_commands(const Dictionary &p_value) { commands = p_value; }
	Dictionary get_commands() const { return commands; }
	void set_signals(const Dictionary &p_value) { signals = p_value; }
	Dictionary get_signals() const { return signals; }
	bool compile(Object *p_target, RNSceneAttachment &r_attachment, RNError &r_error) const;
};
