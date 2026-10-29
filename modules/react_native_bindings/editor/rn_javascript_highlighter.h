#pragma once

#ifdef TOOLS_ENABLED

#include "editor/script/syntax_highlighters.h"
#include "scene/resources/syntax_highlighter.h"

// Highlights the React project's JavaScript and TypeScript in Godot's script
// editor with the editor theme's colors.
class RNJavaScriptSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(RNJavaScriptSyntaxHighlighter, EditorSyntaxHighlighter);

	Ref<CodeHighlighter> highlighter;

public:
	static PackedStringArray extensions() { return { "js", "jsx", "ts", "tsx", "mjs", "cjs" }; }

	void _update_cache() override;
	Dictionary _get_line_syntax_highlighting_impl(int p_line) override { return highlighter->get_line_syntax_highlighting(p_line); }
	PackedStringArray _get_supported_languages() const override { return extensions(); }
	String _get_name() const override { return "JavaScript"; }
	Ref<EditorSyntaxHighlighter> _create() const override;

	RNJavaScriptSyntaxHighlighter() { highlighter.instantiate(); }
};

#endif // TOOLS_ENABLED
