"""Source and include lists for the upstream React Native and third-party code the module compiles."""

import os
import shutil

MODULE_DIR = os.path.dirname(os.path.abspath(__file__))
THIRDPARTY_DIR = os.path.join(MODULE_DIR, "thirdparty")
CONFIGURED_DIR = os.path.join(THIRDPARTY_DIR, "configured")
REACT_NATIVE_DIR = os.path.join(THIRDPARTY_DIR, "react-native", "packages", "react-native")
REACT_COMMON_DIR = os.path.join(REACT_NATIVE_DIR, "ReactCommon")
REACT_CXX_PLATFORM_DIR = os.path.join(REACT_NATIVE_DIR, "ReactCxxPlatform")
REACT_PLATFORM_DIR = os.path.join(MODULE_DIR, "react_platform")
GENERATED_DIR = os.path.join(MODULE_DIR, "generated")
HERMES_DIR = os.path.join(MODULE_DIR, "engines", "hermes")

REACT_COMMON_SOURCE_DIRS = [
    "react/renderer",
    "react/runtime",
    "react/nativemodule/core",
    "react/nativemodule/defaults",
    "react/nativemodule/dom",
    "react/nativemodule/featureflags",
    "react/nativemodule/idlecallbacks",
    "react/nativemodule/intersectionobserver",
    "react/nativemodule/microtasks",
    "react/nativemodule/mutationobserver",
    "react/nativemodule/webperformance",
    "react/nativemodule/viewtransition",
    "react/featureflags",
    "react/bridging",
    "react/debug",
    "react/utils",
    "react/timing",
    "react/performance",
    "jsinspector-modern",
    "jserrorhandler",
    "jsitooling",
    "hermes/inspector-modern",
    "oscompat",
    "reactperflogger",
    "cxxreact",
    "callinvoker",
    "runtimeexecutor",
    "logger",
    "yoga",
]

# Sources that Godot replaces from react_platform/ must leave the build, otherwise
# they link next to their replacement.
REPLACED_SOURCES = {
    "ReactCxxPlatform/react/io/platform/cxx/ResourceLoader.cpp",
    "ReactCxxPlatform/react/http/platform/cxx/WebSocketClient.cpp",
    "ReactCommon/react/renderer/textlayoutmanager/platform/cxx/react/renderer/textlayoutmanager/TextLayoutManager.cpp",
}

EXCLUDED_DIRECTORY_NAMES = {"tests", "__tests__", "iostests", "jni", "android", "ios", "androidswitch"}

FOLLY_SOURCES = [
    "folly/Conv.cpp",
    "folly/Demangle.cpp",
    "folly/FileUtil.cpp",
    "folly/Format.cpp",
    "folly/ScopeGuard.cpp",
    "folly/SharedMutex.cpp",
    "folly/String.cpp",
    "folly/Unicode.cpp",
    "folly/concurrency/CacheLocality.cpp",
    "folly/container/detail/F14Table.cpp",
    "folly/detail/FileUtilDetail.cpp",
    "folly/detail/Futex.cpp",
    "folly/detail/SplitStringSimd.cpp",
    "folly/detail/UniqueInstance.cpp",
    "folly/hash/SpookyHashV2.cpp",
    "folly/io/IOBuf.cpp",
    "folly/json/dynamic.cpp",
    "folly/json/json_pointer.cpp",
    "folly/json/json.cpp",
    "folly/lang/CString.cpp",
    "folly/lang/Exception.cpp",
    "folly/lang/SafeAssert.cpp",
    "folly/lang/ToAscii.cpp",
    "folly/memory/SanitizeAddress.cpp",
    "folly/memory/detail/MallocImpl.cpp",
    "folly/net/NetOps.cpp",
    "folly/portability/SysUio.cpp",
    "folly/synchronization/SanitizeThread.cpp",
    "folly/synchronization/ParkingLot.cpp",
    "folly/system/AtFork.cpp",
    "folly/system/ThreadId.cpp",
    "folly/system/ThreadName.cpp",
]

GLOG_SOURCES = ["demangle.cc", "logging.cc", "raw_logging.cc", "signalhandler.cc", "symbolize.cc", "utilities.cc", "vlog_is_on.cc"]
FMT_SOURCES = ["src/format.cc"]
DOUBLE_CONVERSION_SOURCES = ["bignum.cc", "bignum-dtoa.cc", "cached-powers.cc", "diy-fp.cc", "double-conversion.cc", "fast-dtoa.cc", "fixed-dtoa.cc", "strtod.cc"]

FOLLY_DEFINES = [
    "FOLLY_NO_CONFIG=1",
    "FOLLY_CFG_NO_COROUTINES=1",
    "FOLLY_MOBILE=1",
    "FOLLY_HAVE_CLOCK_GETTIME=1",
    "FOLLY_HAVE_PTHREAD=1",
    "FOLLY_HAVE_XSI_STRERROR_R=1",
]
REACT_NATIVE_DEFINES = FOLLY_DEFINES + ["HERMES_V1_ENABLED=1", "HERMES_ENABLE_DEBUGGER=1", 'LOG_TAG=\\"ReactNative\\"']

# Tokens that the upstream Android build substitutes into glog's configure templates.
GLOG_TOKENS = {
    "ac_cv_have_unistd_h": "1",
    "ac_cv_have_stdint_h": "1",
    "ac_cv_have_systypes_h": "1",
    "ac_cv_have_inttypes_h": "1",
    "ac_cv_have_libgflags": "0",
    "ac_google_start_namespace": "namespace google {",
    "ac_cv_have_uint16_t": "1",
    "ac_cv_have_u_int16_t": "1",
    "ac_cv_have___uint16": "0",
    "ac_google_end_namespace": "}",
    "ac_cv_have___builtin_expect": "1",
    "ac_google_namespace": "google",
    "ac_cv___attribute___noinline": "__attribute__ ((noinline))",
    "ac_cv___attribute___noreturn": "__attribute__ ((noreturn))",
    "ac_cv___attribute___printf_4_5": "__attribute__((__format__ (__printf__, 4, 5)))",
}


def _is_foreign_platform_directory(relative_path):
    parts = relative_path.split("/")
    return any(parts[index] == "platform" and parts[index + 1] != "cxx" for index in range(len(parts) - 1))


def _collect(root, relative_dirs, label):
    sources = []
    for relative_dir in relative_dirs:
        for directory, subdirectories, files in os.walk(os.path.join(root, relative_dir)):
            subdirectories[:] = sorted(name for name in subdirectories if name not in EXCLUDED_DIRECTORY_NAMES)
            relative = os.path.relpath(directory, root).replace(os.sep, "/")
            if _is_foreign_platform_directory(relative):
                subdirectories[:] = []
                continue
            for name in sorted(files):
                path = os.path.join(directory, name)
                if name.endswith(".cpp") and f"{label}/{relative}/{name}" not in REPLACED_SOURCES:
                    sources.append(path)
    return sources


# libjsi from the Hermes build carries only jsi.cpp; these JSI helpers link into the module.
JSI_SOURCES = ["jsi/JSIDynamic.cpp", "jsi/jsilib-posix.cpp"]


def react_native_sources():
    cxx_platform_dirs = sorted(name for name in os.listdir(os.path.join(REACT_CXX_PLATFORM_DIR, "react")) if name not in EXCLUDED_DIRECTORY_NAMES)
    jsi = [os.path.join(REACT_COMMON_DIR, "jsi", path) for path in JSI_SOURCES]
    return jsi + _collect(REACT_COMMON_DIR, REACT_COMMON_SOURCE_DIRS, "ReactCommon") + _collect(
        REACT_CXX_PLATFORM_DIR, ["react/" + name for name in cxx_platform_dirs], "ReactCxxPlatform"
    )


def cpp_sources(root):
    return sorted(os.path.join(directory, name) for directory, _, files in os.walk(root) for name in files if name.endswith(".cpp"))


def generated_sources():
    return cpp_sources(GENERATED_DIR)


def react_platform_sources():
    if not os.path.isdir(REACT_PLATFORM_DIR):
        return []
    return sorted(
        os.path.join(directory, name) for directory, _, files in os.walk(REACT_PLATFORM_DIR) for name in files if name.endswith(".cpp")
    )


def _platform_cxx_include_dirs(root):
    result = []
    for directory, subdirectories, _ in os.walk(root):
        subdirectories[:] = sorted(name for name in subdirectories if name not in EXCLUDED_DIRECTORY_NAMES)
        if os.path.basename(directory) == "cxx" and os.path.basename(os.path.dirname(directory)) == "platform":
            result.append(directory)
            subdirectories[:] = []
    return result


def include_dirs():
    """Godot platform files first, then each upstream platform/cxx, then the common roots."""
    return (
        [REACT_PLATFORM_DIR]
        + _platform_cxx_include_dirs(REACT_COMMON_DIR)
        + _platform_cxx_include_dirs(REACT_CXX_PLATFORM_DIR)
        + [
            GENERATED_DIR,
            REACT_COMMON_DIR,
            os.path.join(REACT_COMMON_DIR, "callinvoker"),
            os.path.join(REACT_COMMON_DIR, "runtimeexecutor"),
            os.path.join(REACT_COMMON_DIR, "react", "nativemodule", "core"),
            os.path.join(REACT_COMMON_DIR, "jsiexecutor"),
            os.path.join(REACT_COMMON_DIR, "jsitooling"),
            os.path.join(REACT_COMMON_DIR, "reactperflogger"),
            os.path.join(REACT_COMMON_DIR, "react", "renderer", "imagemanager"),
            os.path.join(REACT_COMMON_DIR, "yoga"),
            REACT_CXX_PLATFORM_DIR,
        ]
        + third_party_include_dirs()
    )


def third_party_include_dirs():
    return [
        os.path.join(THIRDPARTY_DIR, "folly"),
        CONFIGURED_DIR,
        os.path.join(THIRDPARTY_DIR, "fmt", "include"),
        os.path.join(THIRDPARTY_DIR, "fast_float", "include"),
        os.path.join(THIRDPARTY_DIR, "boost"),
        os.path.join(THIRDPARTY_DIR, "nlohmann_json", "include"),
    ]


def hermes_include_dirs():
    """Hermes is built against React Native's JSI, so JSI headers come from ReactCommon."""
    return [os.path.join(HERMES_DIR, path) for path in ("API", "public", "include")] + [os.path.join(REACT_COMMON_DIR, "jsi")]


def third_party_sources():
    return {
        "folly": [os.path.join(THIRDPARTY_DIR, "folly", path) for path in FOLLY_SOURCES],
        "glog": [os.path.join(THIRDPARTY_DIR, "glog", "src", path) for path in GLOG_SOURCES],
        "fmt": [os.path.join(THIRDPARTY_DIR, "fmt", path) for path in FMT_SOURCES],
        "double-conversion": [os.path.join(THIRDPARTY_DIR, "double-conversion", "src", path) for path in DOUBLE_CONVERSION_SOURCES],
    }


def glog_private_include_dirs():
    return [os.path.join(CONFIGURED_DIR, "glog-private"), os.path.join(THIRDPARTY_DIR, "glog", "src")]


def _write_if_changed(path, content):
    if os.path.isfile(path):
        with open(path, encoding="utf-8") as existing:
            if existing.read() == content:
                return
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as output:
        output.write(content)


def configure_third_party():
    """Writes the configured headers that upstream's Android build prepares for glog and double-conversion."""
    glog_source = os.path.join(THIRDPARTY_DIR, "glog", "src", "glog")
    for name in sorted(os.listdir(glog_source)):
        with open(os.path.join(glog_source, name), encoding="utf-8") as template:
            content = template.read()
        for token, value in GLOG_TOKENS.items():
            content = content.replace("@" + token + "@", value)
        _write_if_changed(os.path.join(CONFIGURED_DIR, "glog", name.removesuffix(".in")), content)
    with open(os.path.join(REACT_NATIVE_DIR, "ReactAndroid", "src", "main", "jni", "third-party", "glog", "config.h"), encoding="utf-8") as config:
        _write_if_changed(os.path.join(CONFIGURED_DIR, "glog-private", "config.h"), config.read())
    double_conversion_source = os.path.join(THIRDPARTY_DIR, "double-conversion", "src")
    for name in sorted(os.listdir(double_conversion_source)):
        if name.endswith(".h"):
            target = os.path.join(CONFIGURED_DIR, "double-conversion", name)
            if not os.path.isfile(target):
                os.makedirs(os.path.dirname(target), exist_ok=True)
                shutil.copyfile(os.path.join(double_conversion_source, name), target)
