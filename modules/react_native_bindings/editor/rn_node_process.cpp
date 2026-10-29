#include "rn_node_process.h"

#ifdef TOOLS_ENABLED

#include "rn_tooling.h"

bool RNNodeProcess::start(const String &p_script, const List<String> &p_arguments) {
	stop();
	List<String> arguments;
	arguments.push_back(p_script);
	for (const String &argument : p_arguments) {
		arguments.push_back(argument);
	}
	const Dictionary process = OS::get_singleton()->execute_with_pipe(rn_node_path(), arguments, false);
	if (process.is_empty()) {
		return false;
	}
	pid = ProcessID(int64_t(process["pid"]));
	output = process["stdio"];
	errors = process["stderr"];
	return true;
}

void RNNodeProcess::stop() {
	if (is_running()) {
		OS::get_singleton()->kill(pid);
	}
	pid = 0;
	output.unref();
	errors.unref();
	pending_output = String();
	pending_errors = String();
}

bool RNNodeProcess::is_running() const {
	return pid != 0 && OS::get_singleton()->is_process_running(pid);
}

int RNNodeProcess::get_exit_code() const {
	return pid == 0 ? 0 : OS::get_singleton()->get_process_exit_code(pid);
}

void RNNodeProcess::_read(const Ref<FileAccess> &p_pipe, String &r_pending, Vector<String> &r_lines) {
	if (p_pipe.is_null()) {
		return;
	}
	for (Vector<uint8_t> chunk = p_pipe->get_buffer(4096); !chunk.is_empty(); chunk = p_pipe->get_buffer(4096)) {
		r_pending += String::utf8(reinterpret_cast<const char *>(chunk.ptr()), chunk.size());
	}
	for (int end = r_pending.find_char('\n'); end != -1; end = r_pending.find_char('\n')) {
		r_lines.push_back(r_pending.substr(0, end).strip_edges(false, true));
		r_pending = r_pending.substr(end + 1);
	}
}

Vector<String> RNNodeProcess::read_lines() {
	Vector<String> lines;
	_read(output, pending_output, lines);
	_read(errors, pending_errors, lines);
	return lines;
}

#endif // TOOLS_ENABLED
