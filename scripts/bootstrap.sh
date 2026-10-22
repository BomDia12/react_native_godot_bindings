#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")

# shellcheck source=../baseline.env
source "$REPO_ROOT/baseline.env"

GODOT_SOURCE_DIR=${GODOT_SOURCE_DIR:-"$REPO_ROOT/godot"}
HERMES_DIR="$REPO_ROOT/modules/react_native_bindings/engines/hermes"
THIRDPARTY_DIR="$REPO_ROOT/modules/react_native_bindings/thirdparty"
CREATED_CHECKOUT=false

if [[ ! -e "$GODOT_SOURCE_DIR" ]]; then
	git clone --filter=blob:none --no-checkout https://github.com/godotengine/godot.git "$GODOT_SOURCE_DIR"
	CREATED_CHECKOUT=true
elif [[ ! -e "$GODOT_SOURCE_DIR/.git" ]]; then
	echo "Godot source path exists but is not a Git checkout: $GODOT_SOURCE_DIR" >&2
	exit 1
fi

if [[ "$CREATED_CHECKOUT" == false && $(git -C "$GODOT_SOURCE_DIR" rev-parse HEAD) != "$GODOT_COMMIT" && -n $(git -C "$GODOT_SOURCE_DIR" status --porcelain) ]]; then
	echo "Godot checkout has local modifications; refusing to change revisions: $GODOT_SOURCE_DIR" >&2
	exit 1
fi

if [[ "$CREATED_CHECKOUT" == true || $(git -C "$GODOT_SOURCE_DIR" rev-parse HEAD 2>/dev/null || true) != "$GODOT_COMMIT" ]]; then
	git -C "$GODOT_SOURCE_DIR" fetch --depth 1 origin "$GODOT_COMMIT"
	git -C "$GODOT_SOURCE_DIR" checkout --detach "$GODOT_COMMIT"
fi

if ! git -C "$GODOT_SOURCE_DIR" diff --quiet HEAD; then
	echo "Godot checkout has local modifications; the module builds against the unmodified pinned commit: $GODOT_SOURCE_DIR" >&2
	exit 1
fi

git -C "$REPO_ROOT" submodule update --init --recursive modules/react_native_bindings/engines/hermes
git -C "$REPO_ROOT" submodule update --init --depth 1 \
	"$THIRDPARTY_DIR/react-native" "$THIRDPARTY_DIR/folly" "$THIRDPARTY_DIR/glog" \
	"$THIRDPARTY_DIR/fmt" "$THIRDPARTY_DIR/double-conversion" "$THIRDPARTY_DIR/fast_float" "$THIRDPARTY_DIR/nlohmann_json"

for entry in "react-native:$REACT_NATIVE_COMMIT" "folly:$FOLLY_COMMIT" "glog:$GLOG_COMMIT" "fmt:$FMT_COMMIT" \
	"double-conversion:$DOUBLE_CONVERSION_COMMIT" "fast_float:$FAST_FLOAT_COMMIT" "nlohmann_json:$NLOHMANN_JSON_COMMIT"; do
	if [[ $(git -C "$THIRDPARTY_DIR/${entry%%:*}" rev-parse HEAD) != "${entry#*:}" ]]; then
		echo "Submodule ${entry%%:*} does not match baseline.env" >&2
		exit 1
	fi
done

BOOST_DIR="$THIRDPARTY_DIR/boost"
if [[ ! -f "$BOOST_DIR/boost/version.hpp" ]]; then
	BOOST_ARCHIVE=$(mktemp)
	trap 'rm -f "$BOOST_ARCHIVE"' EXIT
	curl -sSLf -o "$BOOST_ARCHIVE" "https://archives.boost.io/release/${BOOST_VERSION//_/.}/source/boost_$BOOST_VERSION.tar.gz"
	echo "$BOOST_SHA256  $BOOST_ARCHIVE" | sha256sum --check --status || {
		echo "Boost archive does not match BOOST_SHA256" >&2
		exit 1
	}
	mkdir -p "$BOOST_DIR"
	tar -xzf "$BOOST_ARCHIVE" -C "$BOOST_DIR" --strip-components=1 "boost_$BOOST_VERSION/boost"
fi

if [[ $(git -C "$GODOT_SOURCE_DIR" rev-parse HEAD) != "$GODOT_COMMIT" ]]; then
	echo "Godot checkout does not match GODOT_COMMIT" >&2
	exit 1
fi

if [[ $(git -C "$HERMES_DIR" rev-parse HEAD) != "$HERMES_COMMIT" ]]; then
	echo "Hermes submodule does not match HERMES_COMMIT" >&2
	exit 1
fi

# The pinned engine's installer names the AccessKit version it builds against;
# a marker records the installed one, so a new pin reinstalls it.
ACCESSKIT_DIR="$GODOT_SOURCE_DIR/bin/build_deps/accesskit"
ACCESSKIT_VERSION=$(sed -n 's/^ac_version = "\(.*\)"$/\1/p' "$GODOT_SOURCE_DIR/misc/scripts/install_accesskit.py")
if [[ -z "$ACCESSKIT_VERSION" ]]; then
	echo "Could not read the AccessKit version from install_accesskit.py" >&2
	exit 1
fi
if [[ $(cat "$ACCESSKIT_DIR/.installed-version" 2>/dev/null) != "$ACCESSKIT_VERSION" ]]; then
	(cd "$GODOT_SOURCE_DIR" && python3 misc/scripts/install_accesskit.py)
	echo "$ACCESSKIT_VERSION" > "$ACCESSKIT_DIR/.installed-version"
fi

printf 'Godot: %s\nHermes: %s\nNext: scripts/build_hermes.sh && scripts/build_godot.sh && scripts/run_baseline.sh\n' \
	"$GODOT_SOURCE_DIR" "$HERMES_DIR"
