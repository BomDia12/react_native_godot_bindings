#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(dirname "$SCRIPT_DIR")

# shellcheck source=../baseline.env
source "$REPO_ROOT/baseline.env"

VARIANT=${1:-linux-x86_64-debugger}
MODULE_DIR="$REPO_ROOT/modules/react_native_bindings"
HERMES_SOURCE_DIR="$MODULE_DIR/engines/hermes"
HERMES_BUILD_DIR="$MODULE_DIR/engines/build/$VARIANT"
HOST_COMPILERS="$MODULE_DIR/engines/build/linux-x86_64-debugger/ImportHostCompilers.cmake"
JSI_DIR="$MODULE_DIR/thirdparty/react-native/packages/react-native/ReactCommon/jsi"
ICU_SOURCE_DIR="$MODULE_DIR/thirdparty/icu"
ICU_PREFIX="$MODULE_DIR/engines/build/icu-linux-x86_64"
ICU_BUILD_DIR="$MODULE_DIR/engines/build/icu-linux-x86_64-build"

if [[ ! -d "$HERMES_SOURCE_DIR/.git" && ! -f "$HERMES_SOURCE_DIR/.git" ]]; then
	echo "Hermes submodule is missing. Run scripts/bootstrap.sh." >&2
	exit 1
fi

if [[ $(git -C "$HERMES_SOURCE_DIR" rev-parse HEAD) != "$HERMES_COMMIT" ]]; then
	echo "Hermes checkout does not match HERMES_COMMIT" >&2
	exit 1
fi

build_icu() {
	if [[ -f "$ICU_PREFIX/lib/libicuuc.a" ]]; then
		return
	fi
	if [[ ! -x "$ICU_SOURCE_DIR/source/runConfigureICU" ]]; then
		echo "ICU source is missing. Run scripts/bootstrap.sh." >&2
		exit 1
	fi
	mkdir -p "$ICU_BUILD_DIR"
	(
		cd "$ICU_BUILD_DIR"
		CFLAGS="-O2 -fPIC" CXXFLAGS="-O2 -fPIC" "$ICU_SOURCE_DIR/source/runConfigureICU" Linux \
			--prefix="$ICU_PREFIX" --enable-static --disable-shared --with-data-packaging=static \
			--disable-tests --disable-samples --disable-extras --disable-icuio --disable-layoutex
		make -j"$(nproc)"
		make install
	)
}

case "$VARIANT" in
	linux-x86_64-debugger | linux-x86_64)
		build_icu
		PLATFORM_ARGS=(-DHERMES_USE_STATIC_ICU=ON -DICU_ROOT="$ICU_PREFIX")
		;;
	android-arm64)
		if [[ ! -f "$HOST_COMPILERS" ]]; then
			echo "Build linux-x86_64-debugger first: Android imports its host compilers." >&2
			exit 1
		fi
		PLATFORM_ARGS=(
			-DCMAKE_TOOLCHAIN_FILE="$ANDROID_HOME/ndk/$ANDROID_NDK_VERSION/build/cmake/android.toolchain.cmake"
			-DANDROID_ABI=arm64-v8a
			-DANDROID_PLATFORM="android-$ANDROID_PLATFORM_API"
			-DANDROID_STL=c++_shared
			-DHERMES_UNICODE_LITE=ON
			-DIMPORT_HOST_COMPILERS="$HOST_COMPILERS"
		)
		;;
	*)
		echo "Unknown Hermes variant: $VARIANT. Use linux-x86_64-debugger, linux-x86_64 or android-arm64." >&2
		exit 1
		;;
esac

DEBUGGER=OFF
if [[ $VARIANT == *-debugger ]]; then
	DEBUGGER=ON
fi

cmake \
	-S "$HERMES_SOURCE_DIR" \
	-B "$HERMES_BUILD_DIR" \
	-G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DBUILD_SHARED_LIBS=OFF \
	-DHERMES_BUILD_SHARED_JSI=OFF \
	-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	-DHERMESVM_GCKIND=HADES \
	-DHERMES_ENABLE_DEBUGGER="$DEBUGGER" \
	-DJSI_DIR="$JSI_DIR" \
	-DHERMES_ENABLE_TEST_SUITE=OFF \
	"${PLATFORM_ARGS[@]}"

cmake --build "$HERMES_BUILD_DIR" --target hermesvm_a jsi

for archive in "$HERMES_BUILD_DIR/lib/libhermesvm_a.a" "$HERMES_BUILD_DIR/jsi/libjsi.a"; do
	if [[ ! -f "$archive" ]]; then
		echo "Hermes build did not produce $archive" >&2
		exit 1
	fi
done
