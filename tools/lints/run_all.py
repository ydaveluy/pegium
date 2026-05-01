#!/usr/bin/env python3
"""Recovery lint runner.

Runs every recovery lint sequentially and aggregates exit codes. Each
individual lint can also be invoked directly. The runner exists so
CMake / CI can invoke a single command.

Lints currently registered:
  - check_strict_path.py
  - check_private_comparators.py
  - check_axis_orthogonality.py

Run:
    tools/lints/run_all.py [SOURCE_ROOT]

Exit codes:
    0 - every lint passed
    1 - at least one lint reported a violation
    2 - usage error
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

LINTS = (
    "check_strict_path.py",
    "check_private_comparators.py",
    "check_axis_orthogonality.py",
)


def main(argv: list[str]) -> int:
    source_root = Path(argv[1]) if len(argv) > 1 else Path.cwd()
    if not source_root.is_dir():
        print(f"run_all: source root not found: {source_root}", file=sys.stderr)
        return 2

    here = Path(__file__).resolve().parent
    overall = 0
    for lint in LINTS:
        script = here / lint
        result = subprocess.run(
            [sys.executable, str(script), str(source_root)],
            check=False,
        )
        if result.returncode != 0:
            overall = 1
            print(f"==> {lint} FAILED (exit {result.returncode})", file=sys.stderr)
        else:
            print(f"==> {lint} ok")
    return overall


if __name__ == "__main__":
    sys.exit(main(sys.argv))
