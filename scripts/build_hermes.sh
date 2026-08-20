#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")

# shellcheck source=../baseline.env
source "$REPO_ROOT/baseline.env"

HERMES_SOURCE_DIR="$REPO_ROOT/modules/react_native_bindings/engines/hermes"
HERMES_BUILD_DIR="$REPO_ROOT/modules/react_native_bindings/engines/build_release"

if [[ ! -d "$HERMES_SOURCE_DIR/.git" && ! -f "$HERMES_SOURCE_DIR/.git" ]]; then
	echo "Hermes submodule is missing. Run scripts/bootstrap.sh." >&2
	exit 1
fi

if [[ $(git -C "$HERMES_SOURCE_DIR" rev-parse HEAD) != "$HERMES_COMMIT" ]]; then
	echo "Hermes checkout does not match HERMES_COMMIT" >&2
	exit 1
fi

cmake \
	-S "$HERMES_SOURCE_DIR" \
	-B "$HERMES_BUILD_DIR" \
	-G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DBUILD_SHARED_LIBS=ON \
	-DHERMES_BUILD_SHARED_JSI=ON \
	-DHERMESVM_GCKIND=HADES \
	-DHERMES_ENABLE_TEST_SUITE=OFF

cmake --build "$HERMES_BUILD_DIR" --target hermesvm jsi

for library in "$HERMES_BUILD_DIR/lib/libhermesvm.so" "$HERMES_BUILD_DIR/jsi/libjsi.so"; do
	if [[ ! -f "$library" ]]; then
		echo "Hermes build did not produce $library" >&2
		exit 1
	fi
done
