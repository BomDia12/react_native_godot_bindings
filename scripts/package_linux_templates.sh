#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")
GODOT_SOURCE_DIR=${GODOT_SOURCE_DIR:-"$REPO_ROOT/godot"}
ARTIFACT_DIR="$REPO_ROOT/artifacts"
ARCHIVE_NAME=react-native-godot-templates.tpz

# Godot's export template manager looks for templates under GODOT_VERSION_FULL_CONFIG.
VERSION=$(python3 -I -c '
import runpy, sys
version = runpy.run_path(sys.argv[1])
patch = ".%d" % version["patch"] if version["patch"] else ""
print("%d.%d%s.%s.react_native" % (version["major"], version["minor"], patch, version["status"]))
' "$GODOT_SOURCE_DIR/version.py")

mkdir -p "$ARTIFACT_DIR"
STAGING_ROOT=$(mktemp -d "$ARTIFACT_DIR/.templates-package.XXXXXX")
trap 'rm -rf -- "$STAGING_ROOT"' EXIT
mkdir "$STAGING_ROOT/templates"
for template in debug release; do
	BINARY="$GODOT_SOURCE_DIR/bin/godot.linuxbsd.template_$template.x86_64.react_native"
	if [[ ! -x "$BINARY" ]]; then
		echo "Missing export template: $BINARY. Run scripts/build_godot.sh --release template_$template." >&2
		exit 1
	fi
	cp -p "$BINARY" "$STAGING_ROOT/templates/linux_$template.x86_64"
done
echo "$VERSION" > "$STAGING_ROOT/templates/version.txt"

rm -f "$ARTIFACT_DIR/$ARCHIVE_NAME" "$ARTIFACT_DIR/$ARCHIVE_NAME.sha256"
(
	cd "$STAGING_ROOT"
	zip -qr "$ARTIFACT_DIR/$ARCHIVE_NAME" templates
)
(
	cd "$ARTIFACT_DIR"
	sha256sum "$ARCHIVE_NAME" > "$ARCHIVE_NAME.sha256"
)

echo "Created $ARTIFACT_DIR/$ARCHIVE_NAME for $VERSION"
