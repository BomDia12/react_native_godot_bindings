#!/usr/bin/env bash
# Packs react-native-godot for the rolling prerelease, recording the editor
# archive built from the same commit so the CLI can download it. The archive's
# per-commit name stays on the release after later prereleases replace the rest.
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")
ARTIFACT_DIR="$REPO_ROOT/artifacts"
ARCHIVE_NAME=react-native-godot-linux-x86_64.zip
REPOSITORY_URL=${REPOSITORY_URL:-https://github.com/BomDia12/react_native_godot_bindings.git}
RELEASE_URL=${RELEASE_URL:-https://github.com/BomDia12/react_native_godot_bindings/releases/download/baseline-linux-latest}
source "$REPO_ROOT/baseline.env"

if [[ ! -f "$ARTIFACT_DIR/$ARCHIVE_NAME.sha256" ]]; then
	echo "Run scripts/package_linux_baseline.sh first." >&2
	exit 1
fi
COMMIT=$(git -C "$REPO_ROOT" rev-parse HEAD)
STAGING=$(mktemp -d)
trap 'rm -rf -- "$STAGING"' EXIT

npm pack "$REPO_ROOT/packages/react-native-godot" --pack-destination "$STAGING" --silent > /dev/null
tar -xzf "$STAGING"/react-native-godot-*.tgz -C "$STAGING"
COMMIT="$COMMIT" REPOSITORY_URL="$REPOSITORY_URL" SHA256=$(cut -d' ' -f1 "$ARTIFACT_DIR/$ARCHIVE_NAME.sha256") URL="$RELEASE_URL/react-native-godot-linux-x86_64-${COMMIT::12}.zip" \
GODOT_COMMIT="$GODOT_COMMIT" REACT_NATIVE_VERSION="$REACT_NATIVE_VERSION" PACKAGE_DIR="$STAGING/package" node <<'NODE'
const fs = require('fs');
const path = require('path');
const env = process.env;
const manifestPath = path.join(env.PACKAGE_DIR, 'package.json');
const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
manifest.version = `${manifest.version}-preview.g${env.COMMIT.slice(0, 12)}`;
fs.writeFileSync(manifestPath, JSON.stringify(manifest, null, 2) + '\n');
const platform = {
  repository: env.REPOSITORY_URL,
  bindingsCommit: env.COMMIT,
  godotCommit: env.GODOT_COMMIT,
  reactNative: env.REACT_NATIVE_VERSION,
  editors: {'linux-x86_64': {url: env.URL, sha256: env.SHA256}},
};
fs.writeFileSync(path.join(env.PACKAGE_DIR, 'godot-platform.json'), JSON.stringify(platform, null, 2) + '\n');
NODE
mkdir "$STAGING/out"
npm pack "$STAGING/package" --pack-destination "$STAGING/out" --silent > /dev/null
mv "$STAGING"/out/react-native-godot-*.tgz "$ARTIFACT_DIR/react-native-godot.tgz"
echo "Created $ARTIFACT_DIR/react-native-godot.tgz"
