#pragma once

#ifdef TOOLS_ENABLED

#include "core/io/file_access.h"
#include "core/os/os.h"

// A Node.js tool the editor runs in the background, read a line at a time
// without blocking.
class RNNodeProcess {
	ProcessID pid = 0;
	Ref<FileAccess> output;
	Ref<FileAccess> errors;
	String pending_output;
	String pending_errors;

	static void _read(const Ref<FileAccess> &p_pipe, String &r_pending, Vector<String> &r_lines);

public:
	~RNNodeProcess() { stop(); }

	bool start(const String &p_script, const List<String> &p_arguments);
	void stop();
	bool is_running() const;
	int get_exit_code() const;
	// The complete lines written since the last call, from both pipes.
	Vector<String> read_lines();
};

#endif // TOOLS_ENABLED
