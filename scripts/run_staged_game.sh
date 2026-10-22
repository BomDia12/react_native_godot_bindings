#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")
GODOT_SOURCE_DIR=${GODOT_SOURCE_DIR:-"$REPO_ROOT/godot"}
GODOT_BINARY=$(find "$GODOT_SOURCE_DIR/bin" -maxdepth 1 -type f -name 'godot.linuxbsd.editor.dev.*' -perm -u+x -print -quit)
if [[ -z "$GODOT_BINARY" ]]; then
    echo "Build the pinned dev editor before running the staged game." >&2
    exit 1
fi
TEST_ID=game-staged
LOG_DIR="$REPO_ROOT/artifacts/smoke-logs"
PROJECT_DIR=$(mktemp -d)
trap 'rm -rf "$PROJECT_DIR"' EXIT
mkdir -p "$LOG_DIR"

python3 "$SCRIPT_DIR/stage_game_demo.py" --destination "$PROJECT_DIR"
cp "$SCRIPT_DIR/staged_game_check.gd" "$PROJECT_DIR/"
timeout 300s "$GODOT_BINARY" --headless --editor --path "$PROJECT_DIR" --import > "$LOG_DIR/$TEST_ID-import.log" 2>&1
set +e
timeout 90s "$GODOT_BINARY" --headless --audio-driver Dummy --path "$PROJECT_DIR" --script res://staged_game_check.gd > "$LOG_DIR/$TEST_ID.log" 2>&1
RUN_EXIT=$?
set -e
python3 "$SCRIPT_DIR/check_baseline_log.py" --log "$LOG_DIR/$TEST_ID.log" --allowlist "$REPO_ROOT/documentation/expected-warnings.txt" --exit-code "$RUN_EXIT" --test-id "$TEST_ID"
