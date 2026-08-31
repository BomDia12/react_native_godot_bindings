#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")
GODOT_SOURCE_DIR=${GODOT_SOURCE_DIR:-"$REPO_ROOT/godot"}

mapfile -t GODOT_BINARIES < <(find "$GODOT_SOURCE_DIR/bin" -maxdepth 1 -type f -name 'godot.linuxbsd.editor.*' -perm -u+x -print | sort)
DEV_BINARIES=()
for binary in "${GODOT_BINARIES[@]}"; do
	if [[ $binary == *.editor.dev.* ]]; then
		DEV_BINARIES+=("$binary")
	fi
done

if [[ ${#DEV_BINARIES[@]} -eq 1 ]]; then
	GODOT_BINARY=${DEV_BINARIES[0]}
elif [[ ${#GODOT_BINARIES[@]} -eq 1 ]]; then
	GODOT_BINARY=${GODOT_BINARIES[0]}
else
	echo "Expected one Godot dev editor executable, found ${#DEV_BINARIES[@]} among ${#GODOT_BINARIES[@]} editor binaries" >&2
	exit 1
fi

exec "$GODOT_BINARY" --headless --test --test-case='[ReactNativeBindings]*'
