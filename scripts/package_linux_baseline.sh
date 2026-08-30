#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")
GODOT_SOURCE_DIR=${GODOT_SOURCE_DIR:-"$REPO_ROOT/godot"}
ARTIFACT_DIR="$REPO_ROOT/artifacts"
ARCHIVE_NAME=react-native-godot-linux-x86_64.zip
PACKAGE_NAME=react-native-godot-linux-x86_64

mapfile -t GODOT_BINARIES < <(find "$GODOT_SOURCE_DIR/bin" -maxdepth 1 -type f -name 'godot.linuxbsd.editor.*' -perm -u+x -print)
if [[ ${#GODOT_BINARIES[@]} -ne 1 ]]; then
	echo "Expected exactly one Godot editor executable, found ${#GODOT_BINARIES[@]}." >&2
	exit 1
fi

for library in libhermesvm.so libjsi.so; do
	if [[ ! -f "$GODOT_SOURCE_DIR/bin/$library" ]]; then
		echo "Missing runtime library: $GODOT_SOURCE_DIR/bin/$library" >&2
		exit 1
	fi
done

mkdir -p "$ARTIFACT_DIR"
STAGING_ROOT=$(mktemp -d "$ARTIFACT_DIR/.linux-package.XXXXXX")
trap 'rm -rf -- "$STAGING_ROOT"' EXIT
PACKAGE_DIR="$STAGING_ROOT/$PACKAGE_NAME"
mkdir "$PACKAGE_DIR"
cp -p "${GODOT_BINARIES[0]}" "$PACKAGE_DIR/"
cp -p "$GODOT_SOURCE_DIR/bin/libhermesvm.so" "$GODOT_SOURCE_DIR/bin/libjsi.so" "$PACKAGE_DIR/"

if ! LD_LIBRARY_PATH="$PACKAGE_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ldd "$PACKAGE_DIR/$(basename "${GODOT_BINARIES[0]}")" > "$STAGING_ROOT/ldd.log"; then
	cat "$STAGING_ROOT/ldd.log" >&2
	echo "Could not inspect packaged shared-library dependencies." >&2
	exit 1
fi
cat "$STAGING_ROOT/ldd.log"
if grep -F "not found" "$STAGING_ROOT/ldd.log"; then
	echo "Packaged executable has unresolved shared libraries." >&2
	exit 1
fi

rm -f "$ARTIFACT_DIR/$ARCHIVE_NAME" "$ARTIFACT_DIR/$ARCHIVE_NAME.sha256"
(
	cd "$STAGING_ROOT"
	zip -qr "$ARTIFACT_DIR/$ARCHIVE_NAME" "$PACKAGE_NAME"
)
(
	cd "$ARTIFACT_DIR"
	sha256sum "$ARCHIVE_NAME" > "$ARCHIVE_NAME.sha256"
)

echo "Created $ARTIFACT_DIR/$ARCHIVE_NAME"
