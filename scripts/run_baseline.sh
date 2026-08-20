#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")
GODOT_SOURCE_DIR=${GODOT_SOURCE_DIR:-"$REPO_ROOT/godot"}
SAMPLE_DIR="$REPO_ROOT/samples/view-text"
LOG_PATH=${BASELINE_LOG_PATH:-"$REPO_ROOT/baseline-runtime.log"}

python3 "$SCRIPT_DIR/verify_provenance.py"
python3 "$SCRIPT_DIR/validate_compatibility.py"

GODOT_BINARY=$(find "$GODOT_SOURCE_DIR/bin" -maxdepth 1 -type f -name 'godot.linuxbsd.editor.*' -perm -u+x -print -quit)
if [[ -z "$GODOT_BINARY" ]]; then
	echo "Godot editor binary is missing. Run scripts/build_godot.sh." >&2
	exit 1
fi

npm ci --prefix "$SAMPLE_DIR"
rm -f "$SAMPLE_DIR/dist/godot.bundle.js"
npm run --prefix "$SAMPLE_DIR" build:godot

set +e
timeout 30s "$GODOT_BINARY" --headless --path "$SAMPLE_DIR" res://smoke/SmokeMain.tscn >"$LOG_PATH" 2>&1
RUNTIME_STATUS=$?
set -e

cat "$LOG_PATH"
python3 "$SCRIPT_DIR/check_baseline_log.py" \
	--log "$LOG_PATH" \
	--allowlist "$REPO_ROOT/documentation/expected-warnings.txt" \
	--exit-code "$RUNTIME_STATUS"
