#!/usr/bin/env python3
import os
import resource
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path

from check_baseline_log import validate_log
from smoke_manifest import BundleGroup, ManifestError, SmokeManifest, discover_manifests, group_bundles


REPO_ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class SmokeResult:
    test_id: str
    duration: float
    exit_code: int
    log_path: Path
    failures: tuple[str, ...]


def find_godot_binary(godot_source_dir: Path) -> Path:
    candidates = sorted(
        path
        for path in (godot_source_dir / "bin").glob("godot.linuxbsd.editor.*")
        if path.is_file() and os.access(path, os.X_OK)
    )
    if len(candidates) != 1:
        raise RuntimeError(f"expected exactly one Godot editor executable, found {len(candidates)}")
    return candidates[0]


def run_bundle_command(command: list[str], cwd: Path) -> tuple[float, float]:
    started = time.monotonic()
    subprocess.run(command, cwd=cwd, check=True)
    duration = time.monotonic() - started
    peak_rss_mib = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss / 1024
    return duration, peak_rss_mib


def build_bundles(repo_root: Path, groups: list[BundleGroup]) -> None:
    installed: set[Path] = set()
    for group in groups:
        bundle = group.bundle
        package_dir = repo_root / bundle.package_dir
        if bundle.package_dir not in installed:
            duration, peak_rss_mib = run_bundle_command(["npm", "ci"], package_dir)
            installed.add(bundle.package_dir)
            print(
                f"Installed {bundle.package_dir.as_posix()} in {duration:.1f}s "
                f"(peak child RSS so far: {peak_rss_mib:.0f} MiB)"
            )

        output = repo_root / bundle.output
        if output.exists():
            if not output.is_file():
                raise RuntimeError(f"bundle output is not a file: {bundle.output.as_posix()}")
            output.unlink()

        duration, peak_rss_mib = run_bundle_command(["npm", "run", bundle.script], package_dir)
        if not output.is_file():
            raise RuntimeError(f"bundle build did not create {bundle.output.as_posix()}")
        print(
            f"Built {bundle.output.as_posix()} for {', '.join(group.test_ids)} in {duration:.1f}s "
            f"(peak child RSS so far: {peak_rss_mib:.0f} MiB)"
        )


def run_test(repo_root: Path, godot_binary: Path, manifest: SmokeManifest, log_dir: Path) -> SmokeResult:
    log_path = log_dir / f"{manifest.id}.log"
    command = [
        str(godot_binary),
        "--headless",
        "--path",
        str(repo_root / manifest.project_dir),
        manifest.scene,
    ]
    started = time.monotonic()
    timed_out = False
    try:
        completed = subprocess.run(
            command,
            capture_output=True,
            text=True,
            errors="replace",
            timeout=manifest.timeout_seconds,
            check=False,
        )
        exit_code = completed.returncode
        log = completed.stdout + completed.stderr
    except subprocess.TimeoutExpired as error:
        timed_out = True
        exit_code = 124
        stdout = error.stdout.decode(errors="replace") if isinstance(error.stdout, bytes) else error.stdout or ""
        stderr = error.stderr.decode(errors="replace") if isinstance(error.stderr, bytes) else error.stderr or ""
        log = stdout + stderr

    duration = time.monotonic() - started
    log_path.write_text(log, encoding="utf-8")
    failures = validate_log(
        log=log,
        allowlist_path=repo_root / manifest.allowlist,
        exit_code=exit_code,
        test_id=manifest.id,
    )
    if timed_out:
        failures.insert(0, f"runtime exceeded {manifest.timeout_seconds}s timeout")
    return SmokeResult(
        test_id=manifest.id,
        duration=duration,
        exit_code=exit_code,
        log_path=log_path.relative_to(repo_root),
        failures=tuple(failures),
    )


def summarize(results: list[SmokeResult]) -> bool:
    print("\nSmoke test summary:")
    failed = False
    for result in sorted(results, key=lambda item: item.test_id):
        status = "PASS" if not result.failures else "FAIL"
        print(
            f"- {result.test_id}: {status}, {result.duration:.1f}s, "
            f"exit {result.exit_code}, {result.log_path.as_posix()}"
        )
        for failure in result.failures:
            print(f"  - {failure}")
        failed = failed or bool(result.failures)
    return not failed


def main() -> int:
    try:
        manifests = discover_manifests(REPO_ROOT)
        groups = group_bundles(manifests)
        jobs = int(os.environ.get("SMOKE_JOBS", "2"))
        if jobs < 1:
            raise ValueError("SMOKE_JOBS must be at least 1")
        godot_dir = Path(os.environ.get("GODOT_SOURCE_DIR", REPO_ROOT / "godot")).resolve()
        godot_binary = find_godot_binary(godot_dir)
        log_dir = Path(os.environ.get("SMOKE_LOG_DIR", REPO_ROOT / "artifacts/smoke-logs")).resolve()
        log_dir.mkdir(parents=True, exist_ok=True)
        build_bundles(REPO_ROOT, groups)
    except (ManifestError, OSError, RuntimeError, subprocess.CalledProcessError, ValueError) as error:
        print(f"Smoke setup failed: {error}", file=sys.stderr)
        return 1

    results: list[SmokeResult] = []
    with ThreadPoolExecutor(max_workers=min(jobs, len(manifests))) as executor:
        futures = {
            executor.submit(run_test, REPO_ROOT, godot_binary, manifest, log_dir): manifest.id
            for manifest in manifests
        }
        for future in as_completed(futures):
            try:
                results.append(future.result())
            except Exception as error:
                test_id = futures[future]
                results.append(
                    SmokeResult(
                        test_id=test_id,
                        duration=0.0,
                        exit_code=1,
                        log_path=(log_dir / f"{test_id}.log").relative_to(REPO_ROOT),
                        failures=(f"runner error: {error}",),
                    )
                )

    return 0 if summarize(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
