#!/usr/bin/env bash
# Creates a consumer project the way a developer would, from the published
# react-native-godot package when scripts/package_npm.sh has built it and from
# the source tree otherwise, then runs it from a bundle file and from Metro.
set -euo pipefail
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")
GODOT_SOURCE_DIR=${GODOT_SOURCE_DIR:-"$REPO_ROOT/godot"}
GODOT_BINARY=$(find "$GODOT_SOURCE_DIR/bin" -maxdepth 1 -type f -name 'godot.linuxbsd.editor.dev.*' -perm -u+x -print -quit)
if [[ -z "$GODOT_BINARY" ]]; then
	echo "Build the pinned dev editor before running the consumer check." >&2
	exit 1
fi
LOG_DIR="$REPO_ROOT/artifacts/smoke-logs"
PUBLISHED_PACKAGE="$REPO_ROOT/artifacts/react-native-godot.tgz"
PROJECT_DIR=$(mktemp -d)
PACKAGE_DIR=$(mktemp -d)
trap 'rm -rf "$PROJECT_DIR" "$PACKAGE_DIR"' EXIT
mkdir -p "$LOG_DIR"
export GODOT_EDITOR=$GODOT_BINARY

check() {
	local test_id=$1 allowlist=$2
	shift 2
	set +e
	(cd "$PROJECT_DIR/react_native" && timeout 300s npx react-native run-godot "$@" -- --headless --audio-driver Dummy --script res://consumer_check.gd) > "$LOG_DIR/$test_id.log" 2>&1
	local run_exit=$?
	set -e
	python3 "$SCRIPT_DIR/check_baseline_log.py" --log "$LOG_DIR/$test_id.log" --allowlist "$allowlist" --exit-code "$run_exit" --test-id consumer
}

# Without Metro running, a project that uses the developer server loads the
# bundle file that the previous check built.
check_fallback() {
	set +e
	timeout 300s "$GODOT_BINARY" --path "$PROJECT_DIR" --headless --audio-driver Dummy --script res://consumer_check.gd > "$LOG_DIR/consumer-fallback.log" 2>&1
	local run_exit=$?
	set -e
	python3 "$SCRIPT_DIR/check_baseline_log.py" --log "$LOG_DIR/consumer-fallback.log" --allowlist "$REPO_ROOT/documentation/expected-warnings-metro-fallback.txt" --exit-code "$run_exit" --test-id consumer
}

if [[ -f "$PUBLISHED_PACKAGE" ]]; then
	tar -xzf "$PUBLISHED_PACKAGE" -C "$PACKAGE_DIR"
	PACKAGE_ROOT="$PACKAGE_DIR/package"
else
	PACKAGE_ROOT="$REPO_ROOT/packages/react-native-godot"
fi
node "$PACKAGE_ROOT/cli/init.cjs" --path "$PROJECT_DIR" > "$LOG_DIR/consumer-init.log" 2>&1
if [[ -f "$PUBLISHED_PACKAGE" && ! -f "$PROJECT_DIR/react_native/node_modules/react-native-godot/godot-platform.json" ]]; then
	echo "The consumer project's react-native-godot has no godot-platform.json." >&2
	exit 1
fi
cp "$SCRIPT_DIR/consumer_check.gd" "$PROJECT_DIR/"
(cd "$PROJECT_DIR/react_native" && npx tsc --noEmit)
"$GODOT_BINARY" --headless --editor --import --path "$PROJECT_DIR" > "$LOG_DIR/consumer-import.log" 2>&1
check consumer-file "$REPO_ROOT/documentation/expected-warnings.txt" --no-packager
check_fallback
RN_CONSUMER_EXPECT_METRO=1 check consumer-metro "$REPO_ROOT/documentation/expected-warnings-dev-server.txt"
