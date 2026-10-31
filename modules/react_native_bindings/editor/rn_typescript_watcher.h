#pragma once

#ifdef TOOLS_ENABLED

#include "rn_node_process.h"

struct RNProblem {
	String file;
	int line = 0;
	int column = 0;
	String message;
};

// One `tsc --pretty false` diagnostic line, such as
// `components/App.tsx(3,7): error TS2322: Type 'string' is not ...`.
bool rn_parse_typescript_problem(const String &p_line, RNProblem &r_problem);
// The line `tsc --watch` prints when a compilation has finished.
bool rn_is_typescript_summary(const String &p_line);

// Type-checks the React project with `tsc --watch` while the editor runs.
class RNTypeScriptWatcher {
	RNNodeProcess process;
	Vector<RNProblem> collecting;
	Vector<RNProblem> problems;

public:
	void start();
	void stop() { process.stop(); }
	// Returns true when a compilation has finished since the last call.
	bool poll();
	const Vector<RNProblem> &get_problems() const { return problems; }
};

#endif // TOOLS_ENABLED
