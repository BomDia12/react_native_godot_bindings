#!/usr/bin/env python3
import json
import os
import subprocess
from pathlib import Path

from smoke_manifest import ManifestError, discover_manifests


REPO_ROOT = Path(__file__).resolve().parents[1]


def load_baseline() -> dict[str, str]:
    values = {}
    for raw_line in (REPO_ROOT / "baseline.env").read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if line and not line.startswith("#"):
            key, value = line.split("=", 1)
            values[key] = value
    return values


def git_head(path: Path) -> str | None:
    if not path.exists():
        return None
    result = subprocess.run(
        ["git", "-C", str(path), "rev-parse", "HEAD"],
        check=False,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip() if result.returncode == 0 else None


def check_tracked_file(relative_path: Path, failures: list[str]) -> None:
    display_path = relative_path.as_posix()
    input_path = REPO_ROOT / relative_path
    if not input_path.is_file():
        failures.append(f"missing smoke input: {display_path}")
        return

    tracked = subprocess.run(
        ["git", "-C", str(REPO_ROOT), "ls-files", "--error-unmatch", "--", display_path],
        check=False,
        capture_output=True,
        text=True,
    )
    if tracked.returncode != 0:
        failures.append(f"smoke input is not tracked: {display_path}")

    ignored = subprocess.run(
        ["git", "-C", str(REPO_ROOT), "check-ignore", "--quiet", "--", display_path],
        check=False,
    )
    if ignored.returncode == 0:
        failures.append(f"smoke input is ignored: {display_path}")
    elif ignored.returncode != 1:
        failures.append(f"could not inspect ignore status: {display_path}")


def main() -> int:
    baseline = load_baseline()
    failures = []
    godot_dir = Path(os.environ.get("GODOT_SOURCE_DIR", REPO_ROOT / "godot")).resolve()
    hermes_dir = REPO_ROOT / "modules/react_native_bindings/engines/hermes"
    thirdparty = REPO_ROOT / "modules/react_native_bindings/thirdparty"

    revisions = (
        ("Godot", godot_dir, baseline["GODOT_COMMIT"]),
        ("Hermes", hermes_dir, baseline["HERMES_COMMIT"]),
        ("React Native", thirdparty / "react-native", baseline["REACT_NATIVE_COMMIT"]),
        ("folly", thirdparty / "folly", baseline["FOLLY_COMMIT"]),
        ("glog", thirdparty / "glog", baseline["GLOG_COMMIT"]),
        ("fmt", thirdparty / "fmt", baseline["FMT_COMMIT"]),
        ("double-conversion", thirdparty / "double-conversion", baseline["DOUBLE_CONVERSION_COMMIT"]),
        ("fast_float", thirdparty / "fast_float", baseline["FAST_FLOAT_COMMIT"]),
        ("nlohmann/json", thirdparty / "nlohmann_json", baseline["NLOHMANN_JSON_COMMIT"]),
    )
    for name, path, expected in revisions:
        actual = git_head(path)
        if actual != expected:
            failures.append(f"{name} revision: expected {expected}, got {actual or 'missing'}")

    if godot_dir.exists():
        modified = subprocess.run(["git", "-C", str(godot_dir), "diff", "--quiet", "HEAD"], check=False)
        if modified.returncode != 0:
            failures.append("Godot checkout has local changes; the module builds against the unmodified pinned commit")

    for required in (hermes_dir / "LICENSE", thirdparty / "react-native/LICENSE", thirdparty / "boost/boost/version.hpp"):
        if not required.is_file():
            failures.append(f"missing provenance file: {required.relative_to(REPO_ROOT)}")

    expected_packages = {
        "react": baseline["REACT_VERSION"],
        "react-native": baseline["REACT_NATIVE_VERSION"],
        "metro": baseline["METRO_VERSION"],
        "@react-native/metro-config": baseline["REACT_NATIVE_VERSION"],
        "@react-native/babel-preset": baseline["REACT_NATIVE_VERSION"],
    }

    for package_path in sorted((REPO_ROOT / "samples").glob("*/package.json")):
        package = json.loads(package_path.read_text(encoding="utf-8"))
        lock_path = package_path.with_name("package-lock.json")
        lock = json.loads(lock_path.read_text(encoding="utf-8"))
        declared = {**package.get("dependencies", {}), **package.get("devDependencies", {})}
        for name, expected in expected_packages.items():
            if declared.get(name) != expected:
                failures.append(f"package.json {name}: expected {expected}, got {declared.get(name)}")
            locked = lock.get("packages", {}).get(f"node_modules/{name}", {}).get("version")
            if locked != expected:
                failures.append(f"package-lock.json {name}: expected {expected}, got {locked}")

    if (REPO_ROOT / ".nvmrc").read_text(encoding="utf-8").strip() != baseline["NODE_VERSION"]:
        failures.append(".nvmrc does not match NODE_VERSION")
    if f"scons=={baseline['SCONS_VERSION']}" not in (REPO_ROOT / "requirements-ci.txt").read_text(encoding="utf-8"):
        failures.append("requirements-ci.txt does not match SCONS_VERSION")

    try:
        manifests = discover_manifests(REPO_ROOT)
    except ManifestError as error:
        failures.append(f"smoke manifest validation: {error}")
        manifests = []

    smoke_inputs = {
        path
        for manifest in manifests
        for path in (manifest.manifest_path, *manifest.inputs)
    }
    for relative_path in sorted(smoke_inputs):
        check_tracked_file(relative_path, failures)

    if failures:
        print("Provenance validation failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print("Provenance validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
