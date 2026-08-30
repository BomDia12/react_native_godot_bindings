#!/usr/bin/env python3
import argparse
import re
from pathlib import Path


DIAGNOSTIC_TOKENS = (
    "WARNING:",
    "ERROR:",
    "SCRIPT ERROR:",
    "FATAL:",
    "CRASH",
    "Could not access feature flag",
    "nativeFabricUIManager.",
    "Mounting unrecognized view",
    "RNViewStyle: non-numeric color",
    "Hermes",
)


def load_patterns(path: Path) -> list[re.Pattern[str]]:
    patterns = []
    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        try:
            patterns.append(re.compile(line))
        except re.error as error:
            raise ValueError(f"{path}:{line_number}: invalid regular expression: {error}") from error
    return patterns


def diagnostic_context(lines: list[str], rejected_lines: list[int], preceding: int = 5) -> list[str]:
    ranges: list[tuple[int, int]] = []
    for line_number in sorted(set(rejected_lines)):
        start = max(1, line_number - preceding)
        if ranges and start <= ranges[-1][1] + 1:
            ranges[-1] = (ranges[-1][0], max(ranges[-1][1], line_number))
        else:
            ranges.append((start, line_number))

    rejected = set(rejected_lines)
    rendered = []
    for index, (start, end) in enumerate(ranges):
        if index:
            rendered.append("...")
        for line_number in range(start, end + 1):
            marker = ">" if line_number in rejected else " "
            rendered.append(f"{marker} {line_number}: {lines[line_number - 1]}")
    return rendered


def validate_log(log: str, allowlist_path: Path, exit_code: int, test_id: str) -> list[str]:
    lines = log.splitlines()
    patterns = load_patterns(allowlist_path)
    failures = []

    if exit_code != 0:
        failures.append(f"runtime exited with status {exit_code}")

    success_marker = f"RN_SMOKE_OK: {test_id}"
    success_lines = [line for line in lines if line.startswith("RN_SMOKE_OK:")]
    if success_lines.count(success_marker) != 1:
        failures.append(f"expected exactly one {success_marker} marker")

    wrong_ids = sorted(
        {
            line.removeprefix("RN_SMOKE_OK:").strip() or "missing test ID"
            for line in success_lines
            if line != success_marker
        }
    )
    if wrong_ids:
        failures.append(f"success marker used wrong test ID: {', '.join(wrong_ids)}")
    failure_markers = [line for line in lines if line.startswith("RN_SMOKE_FAILED:")]
    for marker in failure_markers:
        details = marker.removeprefix("RN_SMOKE_FAILED:").strip() or "missing failure details"
        failures.append(f"runtime failure: {details}")

    rejected_lines = []
    for line_number, line in enumerate(lines, 1):
        if not any(token in line for token in DIAGNOSTIC_TOKENS):
            continue
        if any(pattern.fullmatch(line) for pattern in patterns):
            continue
        failures.append(f"line {line_number}: unallowlisted diagnostic")
        rejected_lines.append(line_number)

    if rejected_lines:
        failures.append("diagnostic context:\n" + "\n".join(diagnostic_context(lines, rejected_lines)))
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--allowlist", type=Path, required=True)
    parser.add_argument("--exit-code", type=int, required=True)
    parser.add_argument("--test-id", required=True)
    args = parser.parse_args()

    failures = validate_log(
        log=args.log.read_text(encoding="utf-8", errors="replace"),
        allowlist_path=args.allowlist,
        exit_code=args.exit_code,
        test_id=args.test_id,
    )
    if failures:
        print(f"Smoke log validation failed for {args.test_id}:")
        for failure in failures:
            print(f"- {failure}")
        print(f"Update {args.allowlist} only for reviewed compatibility diagnostics.")
        return 1

    print(f"Smoke log validation passed for {args.test_id}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
