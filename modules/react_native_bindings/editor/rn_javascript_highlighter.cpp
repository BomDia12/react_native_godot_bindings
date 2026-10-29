#include "rn_javascript_highlighter.h"

#ifdef TOOLS_ENABLED

#include "editor/settings/editor_settings.h"

namespace {

const char *CONTROL_FLOW[] = { "break", "case", "catch", "continue", "default", "do", "else", "finally", "for", "if", "return", "switch", "throw", "try", "while", "yield", "await" };
const char *KEYWORDS[] = { "as", "async", "class", "const", "debugger", "delete", "enum", "export", "extends", "false", "from", "function", "implements", "import", "in", "instanceof", "interface", "let", "new", "null", "of", "readonly", "satisfies", "static", "super", "this", "true", "type", "typeof", "undefined", "var", "void", "keyof", "declare", "namespace", "private", "protected", "public" };
const char *TYPES[] = { "any", "bigint", "boolean", "never", "number", "object", "string", "symbol", "unknown", "Array", "Map", "Promise", "Record", "Set", "React" };

} // namespace

void RNJavaScriptSyntaxHighlighter::_update_cache() {
	highlighter->set_text_edit(text_edit);
	highlighter->clear_keyword_colors();
	highlighter->clear_member_keyword_colors();
	highlighter->clear_color_regions();

	highlighter->set_symbol_color(EDITOR_GET("text_editor/theme/highlighting/symbol_color"));
	highlighter->set_number_color(EDITOR_GET("text_editor/theme/highlighting/number_color"));
	highlighter->set_function_color(EDITOR_GET("text_editor/theme/highlighting/function_color"));
	highlighter->set_member_variable_color(EDITOR_GET("text_editor/theme/highlighting/member_variable_color"));

	for (const char *keyword : CONTROL_FLOW) {
		highlighter->add_keyword_color(keyword, EDITOR_GET("text_editor/theme/highlighting/control_flow_keyword_color"));
	}
	for (const char *keyword : KEYWORDS) {
		highlighter->add_keyword_color(keyword, EDITOR_GET("text_editor/theme/highlighting/keyword_color"));
	}
	for (const char *type : TYPES) {
		highlighter->add_keyword_color(type, EDITOR_GET("text_editor/theme/highlighting/base_type_color"));
	}

	const Color string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	highlighter->add_color_region("\"", "\"", string_color);
	highlighter->add_color_region("'", "'", string_color);
	highlighter->add_color_region("`", "`", string_color);
	const Color comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	highlighter->add_color_region("//", "", comment_color, true);
	highlighter->add_color_region("/*", "*/", comment_color);
}

Ref<EditorSyntaxHighlighter> RNJavaScriptSyntaxHighlighter::_create() const {
	Ref<RNJavaScriptSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}

#endif // TOOLS_ENABLED
