#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")

# shellcheck source=../baseline.env
source "$REPO_ROOT/baseline.env"

GODOT_SOURCE_DIR=${GODOT_SOURCE_DIR:-"$REPO_ROOT/godot"}
HERMES_DIR="$REPO_ROOT/modules/react_native_bindings/engines/hermes"
CREATED_CHECKOUT=false

if [[ ! -e "$GODOT_SOURCE_DIR" ]]; then
	git clone --filter=blob:none --no-checkout https://github.com/godotengine/godot.git "$GODOT_SOURCE_DIR"
	CREATED_CHECKOUT=true
elif [[ ! -d "$GODOT_SOURCE_DIR/.git" ]]; then
	echo "Godot source path exists but is not a Git checkout: $GODOT_SOURCE_DIR" >&2
	exit 1
fi

if [[ "$CREATED_CHECKOUT" == false && -n $(git -C "$GODOT_SOURCE_DIR" status --porcelain) ]]; then
	echo "Godot checkout has local modifications; refusing to change revisions: $GODOT_SOURCE_DIR" >&2
	exit 1
fi

if [[ "$CREATED_CHECKOUT" == true || $(git -C "$GODOT_SOURCE_DIR" rev-parse HEAD 2>/dev/null || true) != "$GODOT_COMMIT" ]]; then
	git -C "$GODOT_SOURCE_DIR" fetch --depth 1 origin "$GODOT_COMMIT"
	git -C "$GODOT_SOURCE_DIR" checkout --detach "$GODOT_COMMIT"
fi

git -C "$REPO_ROOT" submodule update --init --recursive modules/react_native_bindings/engines/hermes

if [[ $(git -C "$GODOT_SOURCE_DIR" rev-parse HEAD) != "$GODOT_COMMIT" ]]; then
	echo "Godot checkout does not match GODOT_COMMIT" >&2
	exit 1
fi

if [[ $(git -C "$HERMES_DIR" rev-parse HEAD) != "$HERMES_COMMIT" ]]; then
	echo "Hermes submodule does not match HERMES_COMMIT" >&2
	exit 1
fi

printf 'Godot: %s\nHermes: %s\nNext: scripts/build_hermes.sh && scripts/build_godot.sh && scripts/run_baseline.sh\n' \
	"$GODOT_SOURCE_DIR" "$HERMES_DIR"
