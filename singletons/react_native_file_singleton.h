#pragma once

#include "core/error/error_list.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/string/ustring.h"

class Object;

class ReactNativeFileSingleton : public Object {
	GDCLASS(ReactNativeFileSingleton, Object);

	static ReactNativeFileSingleton *singleton;

	String configured_path;
	String cached_content;
	uint64_t last_modified_time = 0;
	bool file_exists = false;
	uint64_t last_poll_msec = 0;
	uint64_t poll_interval_msec = 500;

	static void _idle_callback();

	void _poll();
	Error _reload_content(bool p_force_emit);
	void _emit_change();

protected:
	static void _bind_methods();

public:
	ReactNativeFileSingleton();
	~ReactNativeFileSingleton() override;

	static ReactNativeFileSingleton *get_singleton();

	void set_monitored_file(const String &p_path);
	String get_monitored_file() const;

	String get_file_content() const;
	bool has_file() const;

	void force_refresh();
};
