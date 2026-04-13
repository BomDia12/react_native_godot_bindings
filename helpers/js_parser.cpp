#include "./js_parser.h"

#include "core/error/error_macros.h"

bool match_string(const String &code, int &pos, const String &match) {
    int match_pos = 0;
    while(code[pos] == match[match_pos]) {
        pos++;
        match_pos++;
        if (match_pos >= match.length()) {
            return true;
        }
        if (pos >= code.length()) {
            return false;
        }
    }
    return false;
}

int get_block_end(const String &code, int start_pos) {
    int pos = start_pos;
    int length = code.length();
    int depth = 0;

    while(true) {
        if (code[pos] == '\n' || code[pos] == ';') {
            return pos;
        }

        if (code[pos] == '{') {
            depth++;
            pos++;
            break;
        }
        pos++;
    }

    while(pos < length) {
        if (code[pos] == '{') {
            depth++;
        } else if (code[pos] == '}') {
            depth--;
            if (depth == 0) {
                return pos;
            }
        }
        pos++;
    }

    return -1; // No matching closing brace found
}

String extract_name(const String &code, int &pos, const String &type) {
    int start_pos = pos;
    String name;
    if (type == "function" || type == "class") {
        while(code[pos] == ' ' || code[pos] == '\n' || code[pos] == '\t' || code[pos] == '\r') {
            start_pos++;
            pos++;
        }
        while(pos < code.length() && (code[pos] != ' ' && code[pos] != '(' && code[pos] != '{' && code[pos] != '\n' && code[pos] != '\t' && code[pos] != '\r')) {
            pos++;
        }
        name = code.substr(start_pos, pos - start_pos);
    } else if (type == "var" || type == "let" || type == "const") {
        while(code[pos] == ' ' || code[pos] == '\n' || code[pos] == '\t' || code[pos] == '\r') {
            start_pos++;
            pos++;
        }
        while(pos < code.length() && (code[pos] != ' ' && code[pos] != '=' && code[pos] != ';' && code[pos] != '\n' && code[pos] != '\t' && code[pos] != '\r')) {
            pos++;
        }
        name = code.substr(start_pos, pos - start_pos);
        while(code[pos] != '=') {
            pos++;
        }
        pos++; // Skip '='
    }

    return name;
}

parsed_block parse_block(const String &code, int &pos, Error &err, const String &type) {
    parsed_block block = { .type = type };

    if (match_string(code, pos, type)) {
        String name = extract_name(code.substr(0, pos), pos, type);
        int end_pos = get_block_end(code, pos);
        if (end_pos != -1) {
            err = ERR_PARSE_ERROR;
            return block;
        }
        block.code = code.substr(pos, end_pos - pos);
        block.name = name;
        pos = end_pos + 1;
        return block;
    } else {
        err = ERR_PARSE_ERROR;
        return block;
    }
}

Vector<parsed_block> parse_code(String code, Error &err) {
    Vector<parsed_block> blocks;
    int pos = 0;
    int length = code.length();

    while (pos < length) {
        if (code[pos] == ' ' || code[pos] == '\n' || code[pos] == '\t' || code[pos] == '\r' || code[pos] == ';') {
            pos++;
            continue;
        }

        if (code[pos] == 'f') {
            parsed_block block = parse_block(code, pos, err, "function");
            if (block.code != "") {
                blocks.push_back(block);
            }
            if (err != OK) {
                return blocks;
            }
        } else if (code[pos] == 'c') {
            parsed_block block = parse_block(code, pos, err, "class");
            if (block.code != "") {
                blocks.push_back(block);
            }
            if (err != OK) {
                return blocks;
            }
        } else if (code[pos] == 'v') {
            parsed_block block = parse_block(code, pos, err, "var");
            if (block.code != "") {
                blocks.push_back(block);
            }
            if (err != OK) {
                return blocks;
            }
        } else if (code[pos] == 'l') {
            parsed_block block = parse_block(code, pos, err, "let");
            if (block.code != "") {
                blocks.push_back(block);
            }
            if (err != OK) {
                return blocks;
            }
        } else if (code[pos] == 'c') {
            parsed_block block = parse_block(code, pos, err, "const");
            if (block.code != "") {
                blocks.push_back(block);
            }
            if (err != OK) {
                return blocks;
            }
        }

        pos++;
    }

    return blocks;
}
