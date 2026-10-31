#pragma once

#ifdef TOOLS_ENABLED

#include "core/string/ustring.h"

// Where the editor finds the React project and the Node.js tools it runs.
String rn_node_path();
String rn_project_root_path();
String rn_package_path(const String &p_relative);
bool rn_has_react_project();
// Opens p_path, a file system or res:// path, in the script editor at a line,
// counted from 1. Files outside the project stay closed.
bool rn_open_source(const String &p_path, int p_line, int p_column);

#endif // TOOLS_ENABLED
