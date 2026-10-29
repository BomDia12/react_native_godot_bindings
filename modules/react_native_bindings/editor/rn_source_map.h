#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"

#include <memory>
#include <vector>

struct RNSourcePosition {
	String source;
	int line = 0;
	int column = 0;
	String name;
};

// A source map (version 3), with indexed maps' sections, that turns bundle
// positions back into source positions.
class RNSourceMap {
	struct Segment {
		int column = 0;
		int source = -1;
		int source_line = 0;
		int source_column = 0;
		int name = -1;
	};
	struct Section {
		int line = 0;
		int column = 0;
		std::unique_ptr<RNSourceMap> map;
	};

	Vector<String> sources;
	Vector<String> names;
	std::vector<std::vector<Segment>> lines;
	std::vector<Section> sections;

	bool _parse(const Dictionary &p_map);

public:
	bool parse(const String &p_json);
	// Lines and columns count from 1, as JavaScript stack frames do.
	bool lookup(int p_line, int p_column, RNSourcePosition &r_position) const;
};
