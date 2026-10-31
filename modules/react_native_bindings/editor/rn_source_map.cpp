#include "rn_source_map.h"

#include "core/io/json.h"

namespace {

int base64_digit(char32_t p_char) {
	static const String DIGITS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	return DIGITS.find_char(p_char);
}

// One base64 VLQ value from p_text at r_position, which it advances.
bool decode_vlq(const String &p_text, int &r_position, int &r_value) {
	int result = 0;
	int shift = 0;
	while (r_position < p_text.length()) {
		const int digit = base64_digit(p_text[r_position++]);
		if (digit < 0 || shift > 30) {
			return false;
		}
		result += (digit & 31) << shift;
		shift += 5;
		if (!(digit & 32)) {
			r_value = (result & 1) ? -(result >> 1) : result >> 1;
			return true;
		}
	}
	return false;
}

} // namespace

bool RNSourceMap::parse(const String &p_json) {
	const Variant map = JSON::parse_string(p_json);
	return map.get_type() == Variant::DICTIONARY && _parse(map);
}

bool RNSourceMap::_parse(const Dictionary &p_map) {
	if (p_map.has("sections")) {
		for (const Variant &entry : Array(p_map["sections"])) {
			const Dictionary section = entry;
			const Dictionary offset = section.get("offset", Dictionary());
			Section parsed{ int(offset.get("line", 0)), int(offset.get("column", 0)), std::make_unique<RNSourceMap>() };
			if (!parsed.map->_parse(section.get("map", Dictionary()))) {
				return false;
			}
			sections.push_back(std::move(parsed));
		}
		return true;
	}
	for (const Variant &source : Array(p_map.get("sources", Array()))) {
		sources.push_back(source);
	}
	for (const Variant &name : Array(p_map.get("names", Array()))) {
		names.push_back(name);
	}
	const String mappings = p_map.get("mappings", "");
	// Every field but the generated column is relative to its last value
	// anywhere in the map; the generated column restarts on each line.
	int column = 0;
	int source = 0;
	int source_line = 0;
	int source_column = 0;
	int name = 0;
	lines.emplace_back();
	for (int position = 0; position < mappings.length();) {
		if (mappings[position] == ';') {
			lines.emplace_back();
			column = 0;
			++position;
			continue;
		}
		if (mappings[position] == ',') {
			++position;
			continue;
		}
		int fields[5] = {};
		int count = 0;
		while (count < 5 && position < mappings.length() && mappings[position] != ',' && mappings[position] != ';') {
			if (!decode_vlq(mappings, position, fields[count++])) {
				return false;
			}
		}
		column += fields[0];
		Segment segment;
		segment.column = column;
		if (count >= 4) {
			source += fields[1];
			source_line += fields[2];
			source_column += fields[3];
			segment.source = source;
			segment.source_line = source_line;
			segment.source_column = source_column;
		}
		if (count == 5) {
			name += fields[4];
			segment.name = name;
		}
		lines.back().push_back(segment);
	}
	return true;
}

bool RNSourceMap::lookup(int p_line, int p_column, RNSourcePosition &r_position) const {
	const int line = p_line - 1;
	const int column = p_column - 1;
	if (!sections.empty()) {
		const Section *found = nullptr;
		for (const Section &section : sections) {
			if (section.line < line || (section.line == line && section.column <= column)) {
				found = &section;
			}
		}
		return found && found->map->lookup(line - found->line + 1, (line == found->line ? column - found->column : column) + 1, r_position);
	}
	if (line < 0 || line >= int(lines.size())) {
		return false;
	}
	const Segment *match = nullptr;
	for (const Segment &segment : lines[line]) {
		if (segment.column > column) {
			break;
		}
		match = &segment;
	}
	if (!match || match->source < 0 || match->source >= sources.size()) {
		return false;
	}
	r_position.source = sources[match->source];
	r_position.line = match->source_line + 1;
	r_position.column = match->source_column + 1;
	r_position.name = match->name >= 0 && match->name < names.size() ? names[match->name] : String();
	return true;
}
