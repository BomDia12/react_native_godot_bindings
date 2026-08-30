import json
import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from smoke_manifest import ManifestError, discover_manifests, group_bundles, load_manifest


class SmokeManifestTests(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name).resolve()
        self.case_dir = self.root / "samples/app/smoke/tests/one"
        self.case_dir.mkdir(parents=True)
        for relative_path in (
            "samples/app/project.godot",
            "samples/app/package.json",
            "samples/app/package-lock.json",
            "samples/app/smoke/tests/one/SmokeMain.tscn",
            "warnings.txt",
        ):
            path = self.root / relative_path
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("{}" if path.suffix == ".json" else "", encoding="utf-8")

    def tearDown(self):
        self.temporary_directory.cleanup()

    def manifest_data(self, test_id="one", mode="shared", output="samples/app/dist/bundle.js"):
        return {
            "id": test_id,
            "project_dir": "samples/app",
            "scene": "res://smoke/tests/one/SmokeMain.tscn",
            "timeout_seconds": 30,
            "allowlist": "warnings.txt",
            "bundle": {
                "mode": mode,
                "package_dir": "samples/app",
                "script": "build:test",
                "output": output,
            },
            "inputs": [
                "samples/app/project.godot",
                "samples/app/package.json",
                "samples/app/package-lock.json",
                "samples/app/smoke/tests/one/SmokeMain.tscn",
                "warnings.txt",
            ],
        }

    def write_manifest(self, data=None, directory=None):
        path = (directory or self.case_dir) / "smoke_test.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(data or self.manifest_data()), encoding="utf-8")
        return path

    def test_loads_exact_manifest(self):
        manifest = load_manifest(self.root, self.write_manifest())
        self.assertEqual(manifest.id, "one")
        self.assertEqual(manifest.scene_path, Path("samples/app/smoke/tests/one/SmokeMain.tscn"))

    def test_rejects_missing_unknown_and_invalid_fields(self):
        missing = self.manifest_data()
        del missing["allowlist"]
        with self.assertRaisesRegex(ManifestError, "missing fields"):
            load_manifest(self.root, self.write_manifest(missing))

        unknown = self.manifest_data()
        unknown["command"] = "anything"
        with self.assertRaisesRegex(ManifestError, "unknown fields"):
            load_manifest(self.root, self.write_manifest(unknown))

        invalid_timeout = self.manifest_data()
        invalid_timeout["timeout_seconds"] = 0
        with self.assertRaisesRegex(ManifestError, "timeout_seconds"):
            load_manifest(self.root, self.write_manifest(invalid_timeout))

    def test_rejects_traversal_and_output_outside_package(self):
        traversal = self.manifest_data()
        traversal["allowlist"] = "../warnings.txt"
        with self.assertRaisesRegex(ManifestError, "traversal"):
            load_manifest(self.root, self.write_manifest(traversal))

        outside = self.manifest_data(output="samples/other/bundle.js")
        with self.assertRaisesRegex(ManifestError, "inside samples/app"):
            load_manifest(self.root, self.write_manifest(outside))

        windows_absolute = self.manifest_data()
        windows_absolute["allowlist"] = "C:/warnings.txt"
        with self.assertRaisesRegex(ManifestError, "absolute"):
            load_manifest(self.root, self.write_manifest(windows_absolute))

    def test_rejects_symlink_escape(self):
        outside = Path(self.temporary_directory.name).with_name(
            f"{Path(self.temporary_directory.name).name}-outside.txt"
        )
        outside.write_text("", encoding="utf-8")
        link = self.root / "escaped.txt"
        link.symlink_to(outside)
        data = self.manifest_data()
        data["allowlist"] = "escaped.txt"
        data["inputs"][-1] = "escaped.txt"
        try:
            with self.assertRaisesRegex(ManifestError, "escapes the repository"):
                load_manifest(self.root, self.write_manifest(data))
        finally:
            outside.unlink()

    def test_groups_shared_bundles_once(self):
        first = load_manifest(self.root, self.write_manifest())
        second_dir = self.root / "samples/app/smoke/tests/two"
        second_scene = second_dir / "SmokeMain.tscn"
        second_scene.parent.mkdir(parents=True)
        second_scene.write_text("", encoding="utf-8")
        data = self.manifest_data(test_id="two")
        data["scene"] = "res://smoke/tests/two/SmokeMain.tscn"
        data["inputs"][3] = "samples/app/smoke/tests/two/SmokeMain.tscn"
        second = load_manifest(self.root, self.write_manifest(data, second_dir))
        groups = group_bundles([first, second])
        self.assertEqual(len(groups), 1)
        self.assertEqual(groups[0].test_ids, ("one", "two"))

    def test_rejects_duplicate_ids_and_dedicated_outputs(self):
        self.write_manifest()
        second_dir = self.root / "samples/app/smoke/tests/two"
        second_scene = second_dir / "SmokeMain.tscn"
        second_scene.parent.mkdir(parents=True)
        second_scene.write_text("", encoding="utf-8")
        duplicate = self.manifest_data()
        duplicate["scene"] = "res://smoke/tests/two/SmokeMain.tscn"
        duplicate["inputs"][3] = "samples/app/smoke/tests/two/SmokeMain.tscn"
        self.write_manifest(duplicate, second_dir)
        with self.assertRaisesRegex(ManifestError, "duplicate smoke test IDs"):
            discover_manifests(self.root)

        first = load_manifest(self.root, self.write_manifest(self.manifest_data(mode="dedicated")))
        duplicate["id"] = "two"
        duplicate["bundle"]["mode"] = "dedicated"
        second = load_manifest(self.root, self.write_manifest(duplicate, second_dir))
        with self.assertRaisesRegex(ManifestError, "not unique"):
            group_bundles([first, second])


if __name__ == "__main__":
    unittest.main()
