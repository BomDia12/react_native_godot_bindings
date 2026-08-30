import contextlib
import io
import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from run_smoke_tests import SmokeResult, summarize


class RunSmokeTestsTests(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
