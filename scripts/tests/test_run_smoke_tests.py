import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from run_smoke_tests import SmokeResult, find_godot_binary, summarize


class RunSmokeTestsTests(unittest.TestCase):
    def test_finds_only_editor_binary(self):
        with tempfile.TemporaryDirectory() as directory:
            godot_dir = Path(directory)
            binary = self._create_binary(godot_dir, "godot.linuxbsd.editor.x86_64")
            self.assertEqual(find_godot_binary(godot_dir), binary)

    def test_prefers_dev_editor_when_release_editor_exists(self):
        with tempfile.TemporaryDirectory() as directory:
            godot_dir = Path(directory)
            self._create_binary(godot_dir, "godot.linuxbsd.editor.x86_64")
            dev_binary = self._create_binary(godot_dir, "godot.linuxbsd.editor.dev.x86_64")
            self.assertEqual(find_godot_binary(godot_dir), dev_binary)

    def test_summary_reports_every_failure(self):
        results = [
            SmokeResult("first", 1.2, 1, Path("first.log"), ("first failure",)),
            SmokeResult("second", 1.4, 2, Path("second.log"), ("second failure",)),
        ]
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            passed = summarize(results)
        self.assertFalse(passed)
        self.assertIn("first failure", output.getvalue())
        self.assertIn("second failure", output.getvalue())

    def _create_binary(self, godot_dir: Path, name: str) -> Path:
        binary = godot_dir / "bin" / name
        binary.parent.mkdir(parents=True, exist_ok=True)
        binary.touch()
        binary.chmod(0o755)
        return binary


if __name__ == "__main__":
    unittest.main()
