#!/usr/bin/env python3
"""Apply and verify the exact additive patch set for the pinned engine."""

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path


PATCH_ROOT = Path(__file__).resolve().parents[1] / "patches/godot"


def git(source: Path, *args: str) -> bytes:
    return subprocess.check_output(["git", "-C", str(source), *args], stderr=subprocess.PIPE)


def validate(source: Path, patch_root: Path = PATCH_ROOT, apply: bool = False) -> str:
    manifest = json.loads((patch_root / "manifest.json").read_text())
    if git(source, "rev-parse", "HEAD").decode().strip() != manifest["base_commit"]:
        raise ValueError("Godot patch base does not match the checkout revision")
    patches = []
    paths = set()
    for entry in manifest["patches"]:
        path = patch_root / entry["file"]
        if path.parent != patch_root or not path.is_file():
            raise ValueError("Invalid Godot patch path")
        content = path.read_bytes()
        if hashlib.sha256(content).hexdigest() != entry["sha256"]:
            raise ValueError(f"Godot patch digest mismatch: {path.name}")
        # Additions/deletions/renames are excluded: the native contract is additive.
        for line in content.decode().splitlines():
            if line.startswith("+++ b/"):
                paths.add(line[6:])
            elif line.startswith("+++ ") or line.startswith("--- /dev/null"):
                raise ValueError("Godot patches must modify existing files")
        patches.append(path.resolve())
    changed = set(filter(None, git(source, "diff", "HEAD", "--name-only", "-z").decode().split("\0")))
    untracked = git(source, "ls-files", "--others", "--exclude-standard", "-z")
    if changed - paths or untracked:
        raise ValueError("Godot checkout has unrelated local changes")
    with tempfile.TemporaryDirectory(prefix="godot-contract-") as temporary:
        expected = Path(temporary)
        git(expected, "init", "-q")
        originals = {}
        for relative in paths:
            if Path(relative).is_absolute() or ".." in Path(relative).parts:
                raise ValueError("Invalid patched source path")
            originals[relative] = git(source, "show", f"HEAD:{relative}")
            output = expected / relative
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(originals[relative])
        for patch in patches:
            git(expected, "apply", str(patch))
        pending = []
        for relative in sorted(paths):
            current = (source / relative).read_bytes()
            patched = (expected / relative).read_bytes()
            if current == patched:
                continue
            if current != originals[relative]:
                raise ValueError(f"Godot source differs from the declared patch: {relative}")
            pending.append((relative, patched))
        if pending and not apply:
            raise ValueError("Godot patches are not applied; run scripts/bootstrap.sh")
        # Validate every file before changing any file.
        for relative, content in pending:
            (source / relative).write_bytes(content)
    return hashlib.sha256((patch_root / "manifest.json").read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    try:
        identity = validate(args.source.resolve(), apply=args.apply)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Godot patch validation failed: {error}\n")
    print(f"Godot patch set verified: {identity}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
