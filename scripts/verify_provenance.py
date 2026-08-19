#!/usr/bin/env python3
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path


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


def yoga_digest(path: Path) -> str:
    digest = hashlib.sha256()
    for source in sorted(item for item in path.rglob("*") if item.is_file()):
        digest.update(source.relative_to(path).as_posix().encode("utf-8"))
        digest.update(b"\0")
        digest.update(source.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def main() -> int:
    baseline = load_baseline()
    failures = []
    godot_dir = Path(os.environ.get("GODOT_SOURCE_DIR", REPO_ROOT / "godot")).resolve()
    hermes_dir = REPO_ROOT / "modules/react_native_bindings/engines/hermes"
    yoga_root = REPO_ROOT / "modules/react_native_bindings/thirdparty/yoga"
    yoga_source = yoga_root / "yoga"

    revisions = (
        ("Godot", godot_dir, baseline["GODOT_COMMIT"]),
        ("Hermes", hermes_dir, baseline["HERMES_COMMIT"]),
    )
    for name, path, expected in revisions:
        actual = git_head(path)
        if actual != expected:
            failures.append(f"{name} revision: expected {expected}, got {actual or 'missing'}")

    for required in (yoga_root / "LICENSE", yoga_root / "UPSTREAM.md", hermes_dir / "LICENSE"):
        if not required.is_file():
            failures.append(f"missing provenance file: {required.relative_to(REPO_ROOT)}")

    upstream_path = yoga_root / "UPSTREAM.md"
    if upstream_path.is_file():
        upstream = upstream_path.read_text(encoding="utf-8")
        revision_match = re.search(r"^Revision: `([0-9a-f]{40})`$", upstream, re.MULTILINE)
        digest_match = re.search(r"^Tree SHA-256: `([0-9a-f]{64})`$", upstream, re.MULTILINE)
        if not revision_match or revision_match.group(1) != baseline["YOGA_REACT_NATIVE_COMMIT"]:
            failures.append("Yoga UPSTREAM.md revision does not match baseline.env")
        actual_digest = yoga_digest(yoga_source)
        if not digest_match or digest_match.group(1) != actual_digest:
            failures.append(f"Yoga tree digest: expected {digest_match.group(1) if digest_match else 'missing'}, got {actual_digest}")

    package_path = REPO_ROOT / "samples/view-text/package.json"
    lock_path = REPO_ROOT / "samples/view-text/package-lock.json"
    package = json.loads(package_path.read_text(encoding="utf-8"))
    lock = json.loads(lock_path.read_text(encoding="utf-8"))
    expected_packages = {
        "react": baseline["REACT_VERSION"],
        "react-native": baseline["REACT_NATIVE_VERSION"],
        "metro": baseline["METRO_VERSION"],
        "@react-native/metro-config": baseline["REACT_NATIVE_VERSION"],
        "@react-native/babel-preset": baseline["REACT_NATIVE_VERSION"],
    }

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

    required_inputs = (
        "modules/react_native_bindings/SCsub",
        "samples/view-text/project.godot",
        "samples/view-text/Main.tscn",
        "samples/view-text/godot.entry.js",
        "samples/view-text/godot.preamble.js",
        "samples/view-text/package-lock.json",
        "samples/view-text/smoke/SmokeMain.tscn",
        "samples/view-text/smoke/smoke_gate.gd",
    )
    for relative_path in required_inputs:
        input_path = REPO_ROOT / relative_path
        if not input_path.is_file():
            failures.append(f"missing baseline input: {relative_path}")
            continue
        ignored = subprocess.run(
            ["git", "-C", str(REPO_ROOT), "check-ignore", "--quiet", "--", relative_path],
            check=False,
        )
        if ignored.returncode == 0:
            failures.append(f"baseline input is ignored: {relative_path}")
        elif ignored.returncode != 1:
            failures.append(f"could not inspect ignore status: {relative_path}")

    if failures:
        print("Provenance validation failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print("Provenance validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
