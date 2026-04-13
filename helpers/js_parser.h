#pragma once

#include "core/templates/vector.h"
#include "core/string/ustring.h"

typedef struct parsed_block {
    String code;
    String name;
    String type;
} parsed_block;

Vector<parsed_block> parse_code(String code, Error &err);
bool match_string(const String &code, int &pos, const String &match);
int get_block_end(const String &code, int start_pos);
parsed_block parse_block(const String &code, int &pos, Error &err, const String &type);
String extract_name(const String &code, int &pos, const String &type);
