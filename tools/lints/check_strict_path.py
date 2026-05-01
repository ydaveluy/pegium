#!/usr/bin/env python3
"""Strict-path effective-dependency lint.

The strict path nominal pays no runtime cost, no semantic cost, and
no effective dependency on the recovery engine: no recovery state is
constructed or consulted there, no branch depends on a recovery datum
before the recovery switch, no recovery function is called. A purely
static common factorization with no runtime cost and no dynamic
dependency on recovery is permitted.

This script enforces the rule by scanning a curated allowlist of
files declared as strict-path-only and verifying they do not
construct or consult any recovery type (RecoveryContext,
BoundaryFacts, RecoveryContract, CandidateEnvelope,
ChoiceRecoverCache, RecoveryPolicyFingerprint).

The allowlist below is enforced: a future change that pulls a
recovery symbol into one of these headers fails CI here, before
silently regressing the strict-path nominal cost guarantee. Adding a
header to the allowlist requires verifying it stays clean of
recovery symbols.

Run:
    tools/lints/check_strict_path.py [SOURCE_ROOT]

Exit codes:
    0 - no violation
    1 - at least one violation
    2 - usage error
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# Files declared as strict-path-only. Each entry is a path relative to
# the source root and must contain no runtime use of any recovery
# symbol. The list is enforced: any future change that pulls a recovery
# symbol into one of these headers fails CI here, before regressing the
# strict-path nominal cost guarantee.
#
# Inclusion criteria: the file is the strict counterpart of a
# strict/recovery duo, exposes only strict-mode parsing utilities, or
# is a pure infrastructure header (skipper, character classification,
# raw value traits) that is consumed on the strict-only nominal path
# and incidentally on the recovery path through derived templates.
STRICT_PATH_FILES: list[str] = [
    "src/pegium/core/parser/Skipper.hpp",
    "src/pegium/core/parser/SkipperBuilder.hpp",
    "src/pegium/core/parser/SkipperContext.hpp",
    "src/pegium/core/parser/AnyCharacter.hpp",
    "src/pegium/core/parser/CharacterRange.hpp",
    "src/pegium/core/parser/RawValueTraits.hpp",
    "src/pegium/core/parser/RuleValue.hpp",
    "src/pegium/core/parser/RuleOptions.hpp",
    "src/pegium/core/parser/CompletionSupport.hpp",
    "src/pegium/core/parser/AssignmentHelpers.hpp",
]

# Symbols whose runtime presence on the strict path is forbidden.
FORBIDDEN_SYMBOLS = (
    "RecoveryContext",
    "BoundaryFacts",
    "RecoveryContract",
    "CandidateEnvelope",
    "ChoiceRecoverCache",
    "RecoveryPolicyFingerprint",
)

# Symbol references appearing inside a comment or in a typedef/alias that
# is purely structural may be ignored. We keep the regex strict for now
# (any occurrence flags the file) and refine when real cases surface.
SYMBOL_RE = re.compile(
    r"\b(" + "|".join(re.escape(s) for s in FORBIDDEN_SYMBOLS) + r")\b"
)


def scan_file(source_root: Path, rel_path: str) -> list[tuple[int, str]]:
    path = source_root / rel_path
    if not path.exists():
        return [(0, f"declared strict-path file does not exist: {rel_path}")]
    violations: list[tuple[int, str]] = []
    with path.open(encoding="utf-8") as fh:
        for line_no, line in enumerate(fh, start=1):
            stripped = line.lstrip()
            if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
                continue
            match = SYMBOL_RE.search(line)
            if match:
                violations.append((line_no, f"forbidden symbol '{match.group(1)}' on strict path"))
    return violations


def main(argv: list[str]) -> int:
    source_root = Path(argv[1]) if len(argv) > 1 else Path.cwd()
    if not source_root.is_dir():
        print(f"check_strict_path: source root not found: {source_root}", file=sys.stderr)
        return 2

    if not STRICT_PATH_FILES:
        # No files declared yet: lint passes trivially.
        return 0

    failed = False
    for rel_path in STRICT_PATH_FILES:
        violations = scan_file(source_root, rel_path)
        if violations:
            failed = True
            for line_no, message in violations:
                print(f"{rel_path}:{line_no}: {message}", file=sys.stderr)

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
