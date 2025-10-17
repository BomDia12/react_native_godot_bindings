#pragma once

#include "core/error/error_list.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

#include <jsi/jsi.h>

class Object;

namespace facebook {
	namespace jsi {
		class Runtime;
		class Value;
		class Object;
	} // namespace jsi
}

typedef struct file_monitor_data {
	String path = "/";
	uint64_t last_modified_time = 0;
	facebook::jsi::Object *processed_result = nullptr;
} file_monitor_data;

class ReactNativeFileSingleton : public Object {
	GDCLASS(ReactNativeFileSingleton, Object);

	static ReactNativeFileSingleton *singleton;

	String base_path = "res://native/";
	Vector<file_monitor_data *> monitored_files;

	Error _reload_data(bool p_force_emit);
	Error _update_file_data(file_monitor_data  * p_data, bool p_force_emit, uint32_t &changes);
	void _emit_change();

protected:
	static void _bind_methods();

public:
	ReactNativeFileSingleton();
	~ReactNativeFileSingleton() override;

	static ReactNativeFileSingleton *get_singleton();

	void add_file_to_monitor(const String &p_path);
	void remove_file_from_monitor(const String &p_path);
	Vector<file_monitor_data *> get_monitored_files() const;

	facebook::jsi::Object * get_file_data(const String &p_path) const;
	bool file_watched(const String &p_path) const;

	void refresh(bool force = false);
};
