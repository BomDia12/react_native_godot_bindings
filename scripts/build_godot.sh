#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")

# shellcheck source=../baseline.env
source "$REPO_ROOT/baseline.env"

GODOT_SOURCE_DIR=${GODOT_SOURCE_DIR:-"$REPO_ROOT/godot"}
MODULE_DIR="$REPO_ROOT/modules/react_native_bindings"

PLATFORM=linuxbsd
TARGET=editor
RELEASE=false
BINARY_SUFFIX=.dev
BUILD_ARGS=(platform=linuxbsd target=editor dev_build=yes tests=yes)
if [[ ${1:-} == --release ]]; then
	RELEASE=true
	BINARY_SUFFIX=
	TARGET=${2:?Usage: scripts/build_godot.sh --release <editor|template_debug|template_release> [--android] [scons args]}
	shift 2
	if [[ ${1:-} == --android ]]; then
		PLATFORM=android
		shift
	fi
	BUILD_ARGS=(platform="$PLATFORM" target="$TARGET" production=yes)
fi

HERMES_VARIANT=linux-x86_64
if [[ $PLATFORM == android ]]; then
	HERMES_VARIANT=android-arm64
	BUILD_ARGS+=(arch=arm64)
fi
if [[ $TARGET != template_release ]]; then
	HERMES_VARIANT+=-debugger
fi
HERMES_BUILD_DIR="$MODULE_DIR/engines/build/$HERMES_VARIANT"

if [[ ! -e "$GODOT_SOURCE_DIR/.git" ]]; then
	echo "Godot checkout is missing. Run scripts/bootstrap.sh." >&2
	exit 1
fi

if [[ $(git -C "$GODOT_SOURCE_DIR" rev-parse HEAD) != "$GODOT_COMMIT" ]]; then
	echo "Godot checkout does not match GODOT_COMMIT" >&2
	exit 1
fi

for module_file in SCsub config.py register_types.cpp register_types.h; do
	if [[ ! -f "$MODULE_DIR/$module_file" ]]; then
		echo "Custom module is incomplete: $MODULE_DIR/$module_file" >&2
		exit 1
	fi
done

for archive in "$HERMES_BUILD_DIR/lib/libhermesvm_a.a" "$HERMES_BUILD_DIR/jsi/libjsi.a"; do
	if [[ ! -f "$archive" ]]; then
		echo "Hermes archive is missing: $archive. Run scripts/build_hermes.sh $HERMES_VARIANT." >&2
		exit 1
	fi
done

command -v scons >/dev/null
command -v "${CXX:-g++}" >/dev/null

# RN_AUTOLINKING names the autolinking.json of a project whose linked native
# modules the editor includes.
CUSTOM_MODULES="$REPO_ROOT/modules"
AUTOLINKING_ARGS=()
if [[ -n "${RN_AUTOLINKING:-}" ]]; then
	RN_AUTOLINKING=$(realpath "$RN_AUTOLINKING")
	while IFS= read -r module; do
		CUSTOM_MODULES+=",$module"
	done < <(python3 -c 'import json, sys; print("\n".join(json.load(open(sys.argv[1]))["modules"]))' "$RN_AUTOLINKING")
	AUTOLINKING_ARGS=("react_native_autolinking=$RN_AUTOLINKING")
fi

cd "$GODOT_SOURCE_DIR"
if [[ $PLATFORM == android ]]; then
	python3 misc/scripts/install_swappy_android.py
fi
scons \
	"${BUILD_ARGS[@]}" \
	warnings=extra \
	werror=yes \
	custom_modules="$CUSTOM_MODULES" \
	custom_modules_recursive=no \
	"${AUTOLINKING_ARGS[@]}" \
	"$@"

if ! grep -q '^#define MODULE_REACT_NATIVE_BINDINGS_ENABLED' "$GODOT_SOURCE_DIR/modules/modules_enabled.gen.h"; then
	echo "Godot disabled the React Native module; check native module dependencies." >&2
	exit 1
fi

if [[ $PLATFORM == android ]]; then
	ANDROID_BUILD_TYPE=release
	if [[ $TARGET != template_release ]]; then
		ANDROID_BUILD_TYPE=debug
	fi
	OUTPUT="$GODOT_SOURCE_DIR/platform/android/java/lib/libs/$ANDROID_BUILD_TYPE/arm64-v8a/libgodot_android.so"
	if [[ ! -f "$OUTPUT" ]]; then
		echo "Godot build did not produce $OUTPUT" >&2
		exit 1
	fi
	READELF=$(find "$ANDROID_HOME/ndk/$ANDROID_NDK_VERSION/toolchains/llvm/prebuilt" -path '*/bin/llvm-readelf' -print -quit)
	if [[ -z "$READELF" ]]; then
		echo "llvm-readelf is missing from Android NDK $ANDROID_NDK_VERSION." >&2
		exit 1
	fi
	DYNAMIC=$("$READELF" -d "$OUTPUT")
	if grep -E 'NEEDED.*\[lib(hermes|jsi|icu)' <<<"$DYNAMIC"; then
		echo "$OUTPUT should link Hermes, JSI and ICU statically." >&2
		exit 1
	fi
	echo "Built $OUTPUT"
	exit 0
fi

# Options such as use_llvm add their own suffix before the module's.
OUTPUT=$(ls -t "$GODOT_SOURCE_DIR/bin"/godot.linuxbsd."$TARGET$BINARY_SUFFIX".x86_64*.react_native 2>/dev/null | head -n 1 || true)
if [[ ! -x "$OUTPUT" ]]; then
	echo "Godot build did not produce a godot.linuxbsd.$TARGET$BINARY_SUFFIX.x86_64*.react_native binary" >&2
	exit 1
fi

if [[ $RELEASE == true ]]; then
	DYNAMIC=$(readelf -d "$OUTPUT")
	UNEXPECTED=$(sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p' <<<"$DYNAMIC" |
		grep -vxE 'libc\.so\.6|libm\.so\.6|libdl\.so\.2|libpthread\.so\.0|librt\.so\.1|ld-linux-x86-64\.so\.2' || true)
	if [[ -n $UNEXPECTED ]]; then
		echo "$OUTPUT should need only glibc, but also needs:" >&2
		echo "$UNEXPECTED" >&2
		exit 1
	fi
fi

echo "Built $OUTPUT"
