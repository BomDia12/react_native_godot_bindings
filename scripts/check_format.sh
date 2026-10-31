#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")
MODULE_DIR="$REPO_ROOT/modules/react_native_bindings"

command -v clang-format >/dev/null

# Third-party submodules, Hermes and codegen output keep their upstream formatting.
mapfile -t SOURCES < <(
	find "$MODULE_DIR" "$REPO_ROOT/samples/libraries" \( -name '*.cpp' -o -name '*.h' \) \
		-not -path "$MODULE_DIR/thirdparty/*" \
		-not -path "$MODULE_DIR/engines/*" \
		-not -path "$MODULE_DIR/generated/*" \
		-not -path '*/node_modules/*' \
		-print | sort
)

if [[ ${#SOURCES[@]} -eq 0 ]]; then
	echo "No module sources found under $MODULE_DIR" >&2
	exit 1
fi

if ! clang-format --dry-run --Werror "${SOURCES[@]}"; then
	echo >&2
	echo "Run 'clang-format -i' over the files listed above." >&2
	exit 1
fi

echo "Formatting matches .clang-format for ${#SOURCES[@]} files"
