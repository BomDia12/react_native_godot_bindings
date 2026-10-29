#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")
GODOT_SOURCE_DIR=${GODOT_SOURCE_DIR:-"$REPO_ROOT/godot"}
GODOT_BINARY=$(find "$GODOT_SOURCE_DIR/bin" -maxdepth 1 -type f -name 'godot.linuxbsd.editor.dev.*' -perm -u+x -print -quit)
if [[ -z "$GODOT_BINARY" ]]; then
    echo "Build the pinned dev editor before running visual checks." >&2
    exit 1
fi
PROJECT_DIR="$REPO_ROOT/samples/game-ui"
OUTPUT_DIR="$REPO_ROOT/artifacts/visual"
mkdir -p "$REPO_ROOT/artifacts/smoke-logs" "$OUTPUT_DIR"
(cd "$PROJECT_DIR" && GODOT_EDITOR=${GODOT_EDITOR:-$GODOT_BINARY} npm run --silent build:fidelity)
"$GODOT_BINARY" --headless --editor --path "$PROJECT_DIR" --import --quit > "$REPO_ROOT/artifacts/smoke-logs/import-visual.log" 2>&1
USER_ARGS=(--visual-output="$OUTPUT_DIR")
if [[ ${RN_VISUAL_UPDATE:-0} == 1 ]]; then
    USER_ARGS+=(--visual-update)
fi
for SCENE in "$PROJECT_DIR"/smoke/visual/*.tscn; do
    APPLICATION=$(basename "$SCENE" .tscn)
    TEST_ID="visual-$APPLICATION"
    VISUAL_LOG="$REPO_ROOT/artifacts/smoke-logs/$TEST_ID.log"
    COMMAND=("$GODOT_BINARY" --path "$PROJECT_DIR" --rendering-method gl_compatibility --rendering-driver opengl3 --audio-driver Dummy "res://smoke/visual/$APPLICATION.tscn" -- "${USER_ARGS[@]}")
    set +e
    if [[ ${COMPONENT_DISPLAY_READY:-0} == 1 ]]; then
        timeout 90s "${COMMAND[@]}" > "$VISUAL_LOG" 2>&1
    else
        command -v xvfb-run >/dev/null || { echo "Install Xvfb to run the visual regression gate." >&2; exit 1; }
        LIBGL_ALWAYS_SOFTWARE=1 timeout 90s xvfb-run -a -s '-screen 0 1280x960x24 -nolisten tcp' "${COMMAND[@]}" > "$VISUAL_LOG" 2>&1
    fi
    VISUAL_EXIT=$?
    set -e
    python3 "$SCRIPT_DIR/check_baseline_log.py" --log "$VISUAL_LOG" --allowlist "$REPO_ROOT/documentation/expected-warnings-component-display.txt" --exit-code "$VISUAL_EXIT" --test-id "$TEST_ID"
done
