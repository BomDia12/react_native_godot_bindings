import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from check_baseline_log import diagnostic_context, validate_log


class CheckBaselineLogTests(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.allowlist = Path(self.temporary_directory.name) / "allowlist.txt"
        self.allowlist.write_text("", encoding="utf-8")

    def tearDown(self):
        self.temporary_directory.cleanup()

    def test_accepts_one_matching_success_marker(self):
        self.assertEqual(validate_log("RN_SMOKE_OK: case\n", self.allowlist, 0, "case"), [])

    def test_rejects_missing_duplicate_and_wrong_markers(self):
        missing = validate_log("", self.allowlist, 0, "case")
        duplicate = validate_log("RN_SMOKE_OK: case\nRN_SMOKE_OK: case\n", self.allowlist, 0, "case")
        wrong = validate_log("RN_SMOKE_OK: other\n", self.allowlist, 0, "case")
        self.assertTrue(any("exactly one" in failure for failure in missing))
        self.assertTrue(any("exactly one" in failure for failure in duplicate))
        self.assertTrue(any("wrong test ID" in failure for failure in wrong))

    def test_rejects_nonzero_exit_and_failure_marker(self):
        failures = validate_log(
            "RN_SMOKE_OK: case\nRN_SMOKE_FAILED: case: broken\n",
            self.allowlist,
            7,
            "case",
        )
        self.assertTrue(any("status 7" in failure for failure in failures))
        self.assertTrue(any("runtime failure: case: broken" in failure for failure in failures))

    def test_allows_full_line_warning_pattern(self):
        self.allowlist.write_text("WARNING: reviewed [0-9]+\n", encoding="utf-8")
        failures = validate_log("WARNING: reviewed 12\nRN_SMOKE_OK: case\n", self.allowlist, 0, "case")
        self.assertEqual(failures, [])

    def test_rejected_diagnostic_has_five_preceding_lines(self):
        lines = [f"line {number}" for number in range(1, 8)] + ["ERROR: rejected"]
        failures = validate_log("\n".join(lines) + "\nRN_SMOKE_OK: case\n", self.allowlist, 0, "case")
        context = next(failure for failure in failures if failure.startswith("diagnostic context"))
        self.assertNotIn(" 2: line 2", context)
        self.assertIn("  3: line 3", context)
        self.assertIn("> 8: ERROR: rejected", context)

    def test_context_at_start_and_overlapping_contexts(self):
        rendered = diagnostic_context(
            ["one", "ERROR: two", "three", "WARNING: four", "five"],
            [2, 4],
        )
        self.assertEqual(rendered.count("  1: one"), 1)
        self.assertNotIn("...", rendered)
        self.assertIn("> 2: ERROR: two", rendered)
        self.assertIn("> 4: WARNING: four", rendered)


if __name__ == "__main__":
    unittest.main()
