#!/usr/bin/env python3
import argparse
import re
from pathlib import Path


def load_patterns(path: Path) -> list[re.Pattern[str]]:
    patterns = []
    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        try:
            patterns.append(re.compile(line))
        except re.error as error:
            raise SystemExit(f"{path}:{line_number}: invalid regular expression: {error}")
    return patterns


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--allowlist", type=Path, required=True)
    parser.add_argument("--exit-code", type=int, required=True)
    args = parser.parse_args()

    log = args.log.read_text(encoding="utf-8", errors="replace")
    lines = log.splitlines()
    patterns = load_patterns(args.allowlist)
    failures = []

    if args.exit_code != 0:
        failures.append(f"runtime exited with status {args.exit_code}")
    if log.count("RN_BASELINE_OK") != 1:
        failures.append("expected exactly one RN_BASELINE_OK marker")
    if "RN_BASELINE_FAILED" in log:
        failures.append("runtime emitted RN_BASELINE_FAILED")

    diagnostic_tokens = (
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

    for line_number, line in enumerate(lines, 1):
        if not any(token in line for token in diagnostic_tokens):
            continue
        if any(pattern.fullmatch(line) for pattern in patterns):
            continue
        failures.append(f"line {line_number}: unallowlisted diagnostic: {line}")

    if failures:
        print("Baseline log validation failed:")
        for failure in failures:
            print(f"- {failure}")
        print(f"Update {args.allowlist} only for reviewed compatibility diagnostics.")
        return 1

    print("Baseline log validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
