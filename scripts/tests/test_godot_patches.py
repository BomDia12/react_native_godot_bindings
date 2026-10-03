import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from godot_patches import validate


class GodotPatchTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.source = Path(self.temporary.name) / "source"
        self.source.mkdir()
        self.patches = Path(self.temporary.name) / "patches"
        self.patches.mkdir()
        self.git("init", "-q")
        self.git("config", "user.email", "fixture@example.invalid")
        self.git("config", "user.name", "Fixture")
        (self.source / "control.cpp").write_text("native default\n")
        (self.source / "other.cpp").write_text("authored\n")
        self.git("add", ".")
        self.git("commit", "-qm", "base")
        (self.source / "control.cpp").write_text("native default\noptional contract\n")
        patch = self.git("diff").stdout
        self.git("restore", "control.cpp")
        (self.patches / "contract.patch").write_bytes(patch)
        manifest = {"base_commit": self.git("rev-parse", "HEAD").stdout.decode().strip(), "patches": [{"file": "contract.patch", "sha256": hashlib.sha256(patch).hexdigest()}]}
        (self.patches / "manifest.json").write_text(json.dumps(manifest))

    def git(self, *args):
        return subprocess.run(["git", "-C", str(self.source), *args], check=True, capture_output=True)

    def test_apply_is_idempotent_and_verification_requires_exact_patch(self):
        with self.assertRaisesRegex(ValueError, "not applied"):
            validate(self.source, self.patches)
        identity = validate(self.source, self.patches, apply=True)
        self.assertEqual(identity, validate(self.source, self.patches, apply=True))
        self.assertEqual(identity, validate(self.source, self.patches))
        (self.source / "control.cpp").write_text("unexpected native edit\n")
        with self.assertRaisesRegex(ValueError, "differs"):
            validate(self.source, self.patches, apply=True)
        self.assertEqual((self.source / "control.cpp").read_text(), "unexpected native edit\n")

    def test_unrelated_edits_are_preserved_before_any_mutation(self):
        for name in ("other.cpp", "untracked.cpp"):
            with self.subTest(name=name):
                (self.source / name).write_text("user edit\n")
                with self.assertRaisesRegex(ValueError, "unrelated"):
                    validate(self.source, self.patches, apply=True)
                self.assertEqual((self.source / "control.cpp").read_text(), "native default\n")
                if name == "other.cpp":
                    self.git("restore", name)

    def test_patch_identity_and_base_are_enforced(self):
        manifest_path = self.patches / "manifest.json"
        manifest = json.loads(manifest_path.read_text())
        manifest["base_commit"] = "0" * 40
        manifest_path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "revision"):
            validate(self.source, self.patches, apply=True)
        manifest["base_commit"] = self.git("rev-parse", "HEAD").stdout.decode().strip()
        manifest_path.write_text(json.dumps(manifest))
        (self.patches / "contract.patch").write_text("modified patch")
        with self.assertRaisesRegex(ValueError, "digest"):
            validate(self.source, self.patches, apply=True)

    def test_git_worktree_is_supported(self):
        worktree = Path(self.temporary.name) / "worktree"
        self.git("worktree", "add", "--detach", str(worktree), "HEAD")
        self.assertTrue((worktree / ".git").is_file())
        validate(worktree, self.patches, apply=True)
        validate(worktree, self.patches)
