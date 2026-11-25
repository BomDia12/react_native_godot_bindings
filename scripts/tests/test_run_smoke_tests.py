import contextlib
import io
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from run_smoke_tests import SmokeResult, find_godot_binary, prepare_project_imports, run_captured, summarize


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

    @mock.patch("run_smoke_tests.run_captured")
    def test_import_preparation_runs_once_per_project_and_writes_logs(self, run):
        run.return_value = mock.Mock(returncode=0, stdout="imported\n", stderr="")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log_dir = root / "logs"
            log_dir.mkdir()
            manifests = [
                mock.Mock(project_dir=Path("sample")),
                mock.Mock(project_dir=Path("sample")),
                mock.Mock(project_dir=Path("other")),
            ]
            prepare_project_imports(root, Path("/godot"), manifests, log_dir)
            self.assertEqual(run.call_count, 2)
            self.assertTrue((log_dir / "import-sample.log").is_file())
            self.assertTrue((log_dir / "import-other.log").is_file())

    @mock.patch("run_smoke_tests.run_captured")
    def test_import_preparation_rejects_import_errors(self, run):
        run.return_value = mock.Mock(returncode=0, stdout="ERROR: broken import\n", stderr="")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log_dir = root / "logs"
            log_dir.mkdir()
            with self.assertRaisesRegex(RuntimeError, "Godot import failed"):
                prepare_project_imports(root, Path("/godot"), [mock.Mock(project_dir=Path("sample"))], log_dir)

    def _create_binary(self, godot_dir: Path, name: str) -> Path:
        binary = godot_dir / "bin" / name
        binary.parent.mkdir(parents=True, exist_ok=True)
        binary.touch()
        binary.chmod(0o755)
        return binary

    def test_timeout_ends_children_that_hold_the_output_open(self) -> None:
        script = "import subprocess, time; subprocess.Popen(['sleep', '60']); time.sleep(60)"
        started = time.monotonic()
        with self.assertRaises(subprocess.TimeoutExpired) as raised:
            run_captured([sys.executable, "-c", script], 0.5)
        self.assertLess(time.monotonic() - started, 30)
        self.assertEqual(raised.exception.stdout, "")


if __name__ == "__main__":
    unittest.main()
