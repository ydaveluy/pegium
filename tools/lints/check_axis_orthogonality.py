#!/usr/bin/env python3
"""Recovery contract axis orthogonality lint.

An admission rule that reads more than three central
`RecoveryContract` axes for a single decision is a signal of analytic
density that should be flagged. Above four axes, the rule must be
re-decomposed or the model must declare a missing axis.

This script enforces the rule by scanning decision/admission function
definitions for conjunctive reads of `RecoveryContract` axes and
emitting a warning above three axes, an error above four. The scan
covers a few well-known function-name patterns:

  - `admit_*`, `is_admissible*`, `check_admission*` (legacy / generic)
  - `is_*_legal\\b\\s*\\(` (closed legality predicates)
  - `decide_*\\b\\s*\\(` (precedence-table dispatch)
  - `should_admit_*` (admission helpers)

Each function body's distinct axis references are counted; the
warning/error thresholds apply per-function. False positives can be
silenced by splitting the function into per-axis helpers, which is
exactly the decomposition the rule asks for.

Run:
    tools/lints/check_axis_orthogonality.py [SOURCE_ROOT]

Exit codes:
    0 - no violation (or only warnings)
    1 - at least one error (more than four axes read in one rule)
    2 - usage error
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# Closed list of central RecoveryContract axes.
CENTRAL_AXES = (
    "CommittedPrefixPolicy",
    "ActiveWindowRelation",
    "BoundarySource",
    "BoundaryAuthority",
    "ScriptOwnership",
    "ScriptProvenance",
    "ContinuationRequirement",
    "ReplayPrefixClass",
)

# Production roots to scan for admission rules.
PRODUCTION_ROOTS = (
    "src/pegium/core/parser",
)

# Decision/admission function name patterns. Any function that
# matches this regex is scanned for its body's axis reads. The
# pattern covers the legacy `admit_*` shape plus the closed
# legality predicates (`is_*_legal`) and precedence dispatchers
# (`decide_*`) the phased migration introduces.
ADMIT_BLOCK_START_RE = re.compile(
    r"\b(?:admit_[A-Za-z_]+|is_admissible[A-Za-z_]*|"
    r"check_admission[A-Za-z_]*|is_[A-Za-z_]+_legal|"
    r"decide_[A-Za-z_]+|should_admit_[A-Za-z_]+)\s*\("
)
# Files that hold the contract definition itself; reads here are
# allowed (the enums and predicates need to mention all axes by
# definition).
ALLOWLIST_DEFINING_FILES = (
    "src/pegium/core/parser/RecoveryContract.hpp",
    "src/pegium/core/parser/CandidateEnvelope.hpp",
)
AXIS_RE = re.compile(r"\b(" + "|".join(re.escape(a) for a in CENTRAL_AXES) + r")\b")

WARN_THRESHOLD = 3
ERROR_THRESHOLD = 4


def scan_file(path: Path) -> tuple[list[tuple[int, str]], list[tuple[int, str]]]:
    warnings: list[tuple[int, str]] = []
    errors: list[tuple[int, str]] = []
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()

    in_block = False
    block_start_line = 0
    block_axes: set[str] = set()
    depth = 0
    for line_no, line in enumerate(lines, start=1):
        if not in_block:
            if ADMIT_BLOCK_START_RE.search(line):
                in_block = True
                block_start_line = line_no
                block_axes = set()
                depth = line.count("{") - line.count("}")
                if depth > 0:
                    # Inline body might appear on the same line.
                    for m in AXIS_RE.finditer(line):
                        block_axes.add(m.group(1))
        else:
            depth += line.count("{") - line.count("}")
            for m in AXIS_RE.finditer(line):
                block_axes.add(m.group(1))
            if depth <= 0:
                count = len(block_axes)
                if count > ERROR_THRESHOLD:
                    errors.append(
                        (
                            block_start_line,
                            f"admission rule reads {count} central axes "
                            f"({', '.join(sorted(block_axes))}); above {ERROR_THRESHOLD} is bloquant",
                        )
                    )
                elif count > WARN_THRESHOLD:
                    warnings.append(
                        (
                            block_start_line,
                            f"admission rule reads {count} central axes "
                            f"({', '.join(sorted(block_axes))}); above {WARN_THRESHOLD} is suspect",
                        )
                    )
                in_block = False
    return warnings, errors


def main(argv: list[str]) -> int:
    source_root = Path(argv[1]) if len(argv) > 1 else Path.cwd()
    if not source_root.is_dir():
        print(
            f"check_axis_orthogonality: source root not found: {source_root}",
            file=sys.stderr,
        )
        return 2

    has_errors = False
    for root in PRODUCTION_ROOTS:
        root_path = source_root / root
        if not root_path.is_dir():
            continue
        for path in sorted(root_path.rglob("*.[hc]pp")):
            rel = path.relative_to(source_root).as_posix()
            if rel in ALLOWLIST_DEFINING_FILES:
                continue
            warnings, errors = scan_file(path)
            for line_no, message in warnings:
                print(f"{rel}:{line_no}: warning: {message}")
            for line_no, message in errors:
                print(f"{rel}:{line_no}: error: {message}", file=sys.stderr)
                has_errors = True

    return 1 if has_errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
