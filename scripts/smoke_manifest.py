#!/usr/bin/env python3
import json
import re
from dataclasses import dataclass
from pathlib import Path, PurePosixPath


MANIFEST_PATTERN = "samples/*/smoke/tests/*/smoke_test.json"
MANIFEST_FIELDS = {
    "id",
    "project_dir",
    "scene",
    "timeout_seconds",
    "allowlist",
    "bundle",
    "inputs",
}
BUNDLE_FIELDS = {"mode", "package_dir", "script", "output"}
ID_PATTERN = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*")
SCRIPT_PATTERN = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.:-]*")


class ManifestError(ValueError):
    pass


@dataclass(frozen=True)
class Bundle:
    mode: str
    package_dir: Path
    script: str
    output: Path


@dataclass(frozen=True)
class SmokeManifest:
    id: str
    manifest_path: Path
    project_dir: Path
    scene: str
    scene_path: Path
    timeout_seconds: int
    allowlist: Path
    bundle: Bundle
    inputs: tuple[Path, ...]


@dataclass(frozen=True)
class BundleGroup:
    bundle: Bundle
    test_ids: tuple[str, ...]


def _relative_path(repo_root: Path, value: object, field: str) -> Path:
    if not isinstance(value, str) or not value:
        raise ManifestError(f"{field} must be a non-empty relative path")

    pure = PurePosixPath(value)
    if (
        pure.is_absolute()
        or re.match(r"^[A-Za-z]:", value)
        or ".." in pure.parts
        or "." in pure.parts
        or pure.as_posix() != value
    ):
        raise ManifestError(f"{field} must not be absolute or contain traversal")
    if "\\" in value:
        raise ManifestError(f"{field} must use forward slashes")

    candidate = (repo_root / Path(*pure.parts)).resolve(strict=False)
    try:
        candidate.relative_to(repo_root)
    except ValueError as error:
        raise ManifestError(f"{field} escapes the repository") from error
    return candidate.relative_to(repo_root)


def _require_exact_fields(data: object, expected: set[str], field: str) -> dict:
    if not isinstance(data, dict):
        raise ManifestError(f"{field} must be an object")
    actual = set(data)
    missing = sorted(expected - actual)
    unknown = sorted(actual - expected)
    if missing:
        raise ManifestError(f"{field} is missing fields: {', '.join(missing)}")
    if unknown:
        raise ManifestError(f"{field} has unknown fields: {', '.join(unknown)}")
    return data


def _require_inside(repo_root: Path, child: Path, parent: Path, field: str) -> None:
    child_resolved = (repo_root / child).resolve(strict=False)
    parent_resolved = (repo_root / parent).resolve(strict=False)
    try:
        child_resolved.relative_to(parent_resolved)
    except ValueError as error:
        raise ManifestError(f"{field} must be inside {parent.as_posix()}") from error


def load_manifest(repo_root: Path, manifest_path: Path) -> SmokeManifest:
    repo_root = repo_root.resolve()
    absolute_manifest = manifest_path if manifest_path.is_absolute() else repo_root / manifest_path
    absolute_manifest = absolute_manifest.resolve(strict=False)
    try:
        relative_manifest = absolute_manifest.relative_to(repo_root)
    except ValueError as error:
        raise ManifestError("manifest path escapes the repository") from error

    expected_parts = relative_manifest.parts
    if (
        len(expected_parts) != 6
        or expected_parts[0] != "samples"
        or expected_parts[2:4] != ("smoke", "tests")
        or expected_parts[5] != "smoke_test.json"
    ):
        raise ManifestError(f"manifest is outside {MANIFEST_PATTERN}")

    try:
        raw = json.loads(absolute_manifest.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ManifestError(f"cannot read {relative_manifest.as_posix()}: {error}") from error
    data = _require_exact_fields(raw, MANIFEST_FIELDS, "manifest")

    test_id = data["id"]
    if not isinstance(test_id, str) or ID_PATTERN.fullmatch(test_id) is None:
        raise ManifestError("id must contain lowercase words separated by hyphens")

    timeout = data["timeout_seconds"]
    if isinstance(timeout, bool) or not isinstance(timeout, int) or not 1 <= timeout <= 600:
        raise ManifestError("timeout_seconds must be an integer from 1 to 600")

    project_dir = _relative_path(repo_root, data["project_dir"], "project_dir")
    samples_dir = Path("samples")
    _require_inside(repo_root, project_dir, samples_dir, "project_dir")
    if project_dir == samples_dir:
        raise ManifestError("project_dir must identify a project below samples/")

    scene = data["scene"]
    if not isinstance(scene, str) or not scene.startswith("res://"):
        raise ManifestError("scene must start with res://")
    scene_relative = _relative_path(repo_root, scene.removeprefix("res://"), "scene")
    scene_path = project_dir / scene_relative
    _require_inside(repo_root, scene_path, project_dir, "scene")

    allowlist = _relative_path(repo_root, data["allowlist"], "allowlist")

    bundle_data = _require_exact_fields(data["bundle"], BUNDLE_FIELDS, "bundle")
    mode = bundle_data["mode"]
    if mode not in {"shared", "dedicated"}:
        raise ManifestError("bundle.mode must be shared or dedicated")
    package_dir = _relative_path(repo_root, bundle_data["package_dir"], "bundle.package_dir")
    _require_inside(repo_root, package_dir, samples_dir, "bundle.package_dir")
    script = bundle_data["script"]
    if not isinstance(script, str) or SCRIPT_PATTERN.fullmatch(script) is None:
        raise ManifestError("bundle.script must be an npm script name")
    output = _relative_path(repo_root, bundle_data["output"], "bundle.output")
    _require_inside(repo_root, output, package_dir, "bundle.output")
    if output == package_dir:
        raise ManifestError("bundle.output must be a file below bundle.package_dir")

    raw_inputs = data["inputs"]
    if not isinstance(raw_inputs, list) or not raw_inputs:
        raise ManifestError("inputs must be a non-empty array")
    inputs = tuple(_relative_path(repo_root, item, "inputs[]") for item in raw_inputs)
    if len(inputs) != len(set(inputs)):
        raise ManifestError("inputs must not contain duplicates")

    required_inputs = {
        scene_path,
        allowlist,
        project_dir / "project.godot",
        package_dir / "package.json",
        package_dir / "package-lock.json",
    }
    missing_inputs = sorted(path.as_posix() for path in required_inputs - set(inputs))
    if missing_inputs:
        raise ManifestError(f"inputs is missing declared files: {', '.join(missing_inputs)}")

    return SmokeManifest(
        id=test_id,
        manifest_path=relative_manifest,
        project_dir=project_dir,
        scene=scene,
        scene_path=scene_path,
        timeout_seconds=timeout,
        allowlist=allowlist,
        bundle=Bundle(mode=mode, package_dir=package_dir, script=script, output=output),
        inputs=inputs,
    )


def group_bundles(manifests: list[SmokeManifest]) -> list[BundleGroup]:
    groups: list[BundleGroup] = []
    shared: dict[tuple[Path, str, Path], list[str]] = {}
    shared_bundles: dict[tuple[Path, str, Path], Bundle] = {}
    output_owners: dict[Path, tuple[str, str, Path] | str] = {}

    for manifest in manifests:
        bundle = manifest.bundle
        if bundle.mode == "shared":
            key = (bundle.package_dir, bundle.script, bundle.output)
            owner: tuple[str, str, Path] | str = (
                bundle.package_dir.as_posix(),
                bundle.script,
                bundle.output,
            )
            previous = output_owners.setdefault(bundle.output, owner)
            if previous != owner:
                raise ManifestError(f"conflicting bundle output: {bundle.output.as_posix()}")
            shared.setdefault(key, []).append(manifest.id)
            shared_bundles[key] = bundle
        else:
            if bundle.output in output_owners:
                raise ManifestError(f"dedicated bundle output is not unique: {bundle.output.as_posix()}")
            output_owners[bundle.output] = manifest.id
            groups.append(BundleGroup(bundle=bundle, test_ids=(manifest.id,)))

    groups.extend(
        BundleGroup(bundle=shared_bundles[key], test_ids=tuple(ids))
        for key, ids in shared.items()
    )
    return sorted(groups, key=lambda group: group.test_ids)


def discover_manifests(repo_root: Path) -> list[SmokeManifest]:
    repo_root = repo_root.resolve()
    manifests = [load_manifest(repo_root, path) for path in sorted(repo_root.glob(MANIFEST_PATTERN))]
    if not manifests:
        raise ManifestError(f"no smoke manifests found at {MANIFEST_PATTERN}")

    ids = [manifest.id for manifest in manifests]
    duplicate_ids = sorted({test_id for test_id in ids if ids.count(test_id) > 1})
    if duplicate_ids:
        raise ManifestError(f"duplicate smoke test IDs: {', '.join(duplicate_ids)}")
    group_bundles(manifests)
    return manifests
