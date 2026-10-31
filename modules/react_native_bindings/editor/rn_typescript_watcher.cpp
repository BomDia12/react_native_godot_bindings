#include "rn_typescript_watcher.h"

#ifdef TOOLS_ENABLED

#include "rn_tooling.h"

#include "core/io/dir_access.h"

bool rn_parse_typescript_problem(const String &p_line, RNProblem &r_problem) {
	const int position = p_line.find("): error TS");
	const int open = position == -1 ? -1 : p_line.rfind_char('(', position);
	if (open <= 0) {
		return false;
	}
	const PackedStringArray location = p_line.substr(open + 1, position - open - 1).split(",");
	if (location.size() != 2 || !location[0].is_valid_int() || !location[1].is_valid_int()) {
		return false;
	}
	r_problem.file = p_line.substr(0, open);
	r_problem.line = location[0].to_int();
	r_problem.column = location[1].to_int();
	r_problem.message = p_line.substr(position + 3);
	return true;
}

bool rn_is_typescript_summary(const String &p_line) {
	return p_line.contains(" - Found ") && p_line.contains("Watching for file changes");
}

void RNTypeScriptWatcher::start() {
	const String project = rn_project_root_path();
	if (!FileAccess::exists(project.path_join("tsconfig.json")) || !FileAccess::exists(project.path_join("node_modules/typescript/bin/tsc"))) {
		return;
	}
	List<String> arguments;
	for (const char *argument : { "--noEmit", "--watch", "--pretty", "false", "-p" }) {
		arguments.push_back(argument);
	}
	arguments.push_back(project);
	process.start(project.path_join("node_modules/typescript/bin/tsc"), arguments);
}

bool RNTypeScriptWatcher::poll() {
	bool finished = false;
	for (const String &line : process.read_lines()) {
		RNProblem problem;
		if (rn_parse_typescript_problem(line, problem)) {
			// tsc names files relative to its working directory, which is the editor's.
			problem.file = DirAccess::create(DirAccess::ACCESS_FILESYSTEM)->get_current_dir().path_join(problem.file).simplify_path();
			collecting.push_back(problem);
		} else if (line.begins_with(" ") && !collecting.is_empty()) {
			collecting.write[collecting.size() - 1].message += "\n" + line.strip_edges();
		} else if (rn_is_typescript_summary(line)) {
			problems = collecting;
			collecting.clear();
			finished = true;
		}
	}
	return finished;
}

#endif // TOOLS_ENABLED
