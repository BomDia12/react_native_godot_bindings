#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")
GODOT_SOURCE_DIR=${GODOT_SOURCE_DIR:-"$REPO_ROOT/godot"}
GODOT_BINARY=$(find "$GODOT_SOURCE_DIR/bin" -maxdepth 1 -type f -name 'godot.linuxbsd.editor.dev.*' -perm -u+x -print -quit)
if [[ -z "$GODOT_BINARY" ]]; then
    echo "Build the pinned dev editor before running display checks." >&2
    exit 1
fi
mkdir -p "$REPO_ROOT/artifacts/smoke-logs"
for CASE in component_contracts presentations theme_geometry; do
    TEST_ID="game-${CASE//_/-}"
    DISPLAY_LOG="$REPO_ROOT/artifacts/smoke-logs/$TEST_ID-display.log"
    COMMAND=("$GODOT_BINARY" --path "$REPO_ROOT/samples/game-ui" --rendering-method gl_compatibility --rendering-driver opengl3 --audio-driver Dummy "res://smoke/tests/$CASE/SmokeMain.tscn")
    set +e
    if [[ ${COMPONENT_DISPLAY_READY:-0} == 1 ]]; then
        timeout 90s "${COMMAND[@]}" > "$DISPLAY_LOG" 2>&1
    else
        command -v xvfb-run >/dev/null || { echo "Install Xvfb to run the native display gate." >&2; exit 1; }
        LIBGL_ALWAYS_SOFTWARE=1 timeout 90s xvfb-run -a -s '-screen 0 1280x960x24 -nolisten tcp' "${COMMAND[@]}" > "$DISPLAY_LOG" 2>&1
    fi
    DISPLAY_EXIT=$?
    set -e
    python3 "$SCRIPT_DIR/check_baseline_log.py" --log "$DISPLAY_LOG" --allowlist "$REPO_ROOT/documentation/expected-warnings-component-display.txt" --exit-code "$DISPLAY_EXIT" --test-id "$TEST_ID"
done
