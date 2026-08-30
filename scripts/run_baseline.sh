#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
python3 "$SCRIPT_DIR/verify_provenance.py"
exec python3 "$SCRIPT_DIR/run_smoke_tests.py" "$@"
