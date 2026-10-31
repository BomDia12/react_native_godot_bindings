#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")

# shellcheck source=../baseline.env
source "$REPO_ROOT/baseline.env"

GODOT_SOURCE_DIR=${GODOT_SOURCE_DIR:-"$REPO_ROOT/godot"}
MODULE_DIR="$REPO_ROOT/modules/react_native_bindings"
HERMES_BUILD_DIR="$MODULE_DIR/engines/build_release"

if [[ ! -e "$GODOT_SOURCE_DIR/.git" ]]; then
	echo "Godot checkout is missing. Run scripts/bootstrap.sh." >&2
	exit 1
fi

if [[ $(git -C "$GODOT_SOURCE_DIR" rev-parse HEAD) != "$GODOT_COMMIT" ]]; then
	echo "Godot checkout does not match GODOT_COMMIT" >&2
	exit 1
fi

for module_file in SCsub config.py register_types.cpp register_types.h; do
	if [[ ! -f "$MODULE_DIR/$module_file" ]]; then
		echo "Custom module is incomplete: $MODULE_DIR/$module_file" >&2
		exit 1
	fi
done

for library in "$HERMES_BUILD_DIR/lib/libhermesvm.so" "$HERMES_BUILD_DIR/jsi/libjsi.so"; do
	if [[ ! -f "$library" ]]; then
		echo "Hermes library is missing: $library. Run scripts/build_hermes.sh." >&2
		exit 1
	fi
done

command -v scons >/dev/null
command -v "${CXX:-g++}" >/dev/null

# RN_AUTOLINKING names the autolinking.json of a project whose linked native
# modules the editor includes.
CUSTOM_MODULES="$REPO_ROOT/modules"
AUTOLINKING_ARGS=()
if [[ -n "${RN_AUTOLINKING:-}" ]]; then
	RN_AUTOLINKING=$(realpath "$RN_AUTOLINKING")
	while IFS= read -r module; do
		CUSTOM_MODULES+=",$module"
	done < <(python3 -c 'import json, sys; print("\n".join(json.load(open(sys.argv[1]))["modules"]))' "$RN_AUTOLINKING")
	AUTOLINKING_ARGS=("react_native_autolinking=$RN_AUTOLINKING")
fi

cd "$GODOT_SOURCE_DIR"
scons \
	platform=linuxbsd \
	target=editor \
	dev_build=yes \
	tests=yes \
	warnings=extra \
	werror=yes \
	custom_modules="$CUSTOM_MODULES" \
	custom_modules_recursive=no \
	"${AUTOLINKING_ARGS[@]}" \
	"$@"

if ! grep -q '^#define MODULE_REACT_NATIVE_BINDINGS_ENABLED' "$GODOT_SOURCE_DIR/modules/modules_enabled.gen.h"; then
	echo "Godot disabled the React Native module; check native module dependencies." >&2
	exit 1
fi

GODOT_BINARY=$(find "$GODOT_SOURCE_DIR/bin" -maxdepth 1 -type f -name 'godot.linuxbsd.editor.*' -perm -u+x -print -quit)
if [[ -z "$GODOT_BINARY" ]]; then
	echo "Godot build did not produce an editor binary" >&2
	exit 1
fi

for output in "$GODOT_SOURCE_DIR/bin/libhermesvm.so" "$GODOT_SOURCE_DIR/bin/libjsi.so"; do
	if [[ ! -f "$output" ]]; then
		echo "Godot build did not copy the runtime dependency: $output" >&2
		exit 1
	fi
done

echo "Built $GODOT_BINARY"
