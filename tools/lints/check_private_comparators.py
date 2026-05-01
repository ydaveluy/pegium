#!/usr/bin/env python3
"""Private comparator lint.

No `RecoveryKey` shadow comparator may live in a combinator. The only
shared ranking authority is `RecoveryKey` through
`is_better_recovery_key`. Every comparator that orders recovery
candidates must delegate to it.

This script enforces the rule by scanning combinator headers for
`operator<`, `operator>`, `operator<=`, `operator>=`, and
`operator<=>` definitions inside namespaces or classes that handle
recovery candidates. The known shared comparator
(`is_better_recovery_key` and its helpers in RecoveryCandidate.hpp)
is allowlisted.

Run:
    tools/lints/check_private_comparators.py [SOURCE_ROOT]

Exit codes:
    0 - no violation
    1 - at least one violation
    2 - usage error
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# Combinator and recovery files inspected for private comparators.
INSPECTED_FILES = (
    "src/pegium/core/parser/OrderedChoice.hpp",
    "src/pegium/core/parser/Group.hpp",
    "src/pegium/core/parser/Repetition.hpp",
    "src/pegium/core/parser/InfixRule.hpp",
    "src/pegium/core/parser/InfixRuleSupport.hpp",
    "src/pegium/core/parser/RecoverySearch.cpp",
    "src/pegium/core/parser/RecoverySearch.hpp",
    "src/pegium/core/parser/EditableRecoverySupport.hpp",
    "src/pegium/core/parser/RecoveryEditSupport.hpp",
    "src/pegium/core/parser/ChoiceAttempt.hpp",
)

# Files that legitimately house the shared comparator authority.
ALLOWLIST_AUTHORITIES = (
    "src/pegium/core/parser/RecoveryCandidate.hpp",
)

# Pattern: an operator definition in C++ source. We match the body
# (a definition with `{`) to exclude declarations in concept/SFINAE
# constraints. `<=>` is the C++20 spaceship operator.
OPERATOR_DEF_RE = re.compile(
    r"\b(?:bool|auto|int|std::strong_ordering|std::weak_ordering|"
    r"std::partial_ordering)\s+operator\s*(<=?>?|>=?)\s*\(",
)

# Pattern: a friend operator definition, common in C++ templated types.
FRIEND_OPERATOR_DEF_RE = re.compile(
    r"\bfriend\s+(?:bool|auto|std::strong_ordering|std::weak_ordering|"
    r"std::partial_ordering)\s+operator\s*(<=?>?|>=?)\s*\(",
)

# A private candidate-ordering function (named, not operator-overloaded)
# is also flagged unless it lives in RecoveryCandidate.hpp. This catches
# the historical pattern of helpers like `extends_same_repaired_prefix`
# being promoted into pairwise candidate selection. The lint matches a
# function call (name + `(`) so a doc comment mentioning the old name is
# fine but a re-introduction in code is flagged.
#
# `is_better_*` helpers are exempt when they live in
# `RecoveryCandidate.hpp` (the central comparator authority); other
# files re-introducing them are flagged. The historical inline
# `extends_*` lambdas were renamed to explicit per-anchor
# family-redundancy filters
# (`extension_outranks_anchor_base`,
# `boundary_repair_outranks_no_edit_iteration`,
# `destructive_extension_outranks_anchor_base`). These are NOT
# replay-equivalence dominance — they remove redundancy inside the
# same-anchor extension family before the central `RecoveryKey`
# ranking. The historical names must never come back.
_FORBIDDEN_HELPER_NAMES = (
    "extends_same_repaired_prefix",
    "extends_no_edit_iteration_boundary",
    "is_better_than",
    "prefer_candidate",
    "select_winning_candidate",
)
# A function call: name followed by `(`.
SUSPICIOUS_HELPER_CALL_RE = re.compile(
    r"(?<![A-Za-z0-9_])("
    + "|".join(_FORBIDDEN_HELPER_NAMES)
    + r")\s*\(",
)
# A lambda or function definition / declaration / assignment: name
# followed by `=` (lambda or alias) or by `(` later on the same line
# is already caught by the call regex; the assignment form catches
# `auto extends_same_repaired_prefix = [](...)`.
SUSPICIOUS_HELPER_DEFINITION_RE = re.compile(
    r"(?<![A-Za-z0-9_])("
    + "|".join(_FORBIDDEN_HELPER_NAMES)
    + r")\s*=",
)


def scan_file(source_root: Path, rel_path: str) -> list[tuple[int, str]]:
    if rel_path in ALLOWLIST_AUTHORITIES:
        return []
    path = source_root / rel_path
    if not path.exists():
        # Missing optional file: not an error.
        return []
    violations: list[tuple[int, str]] = []
    with path.open(encoding="utf-8") as fh:
        for line_no, line in enumerate(fh, start=1):
            stripped = line.lstrip()
            if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
                continue
            if OPERATOR_DEF_RE.search(line) or FRIEND_OPERATOR_DEF_RE.search(line):
                violations.append(
                    (line_no, "private comparison operator defined outside RecoveryCandidate")
                )
            helper_match = SUSPICIOUS_HELPER_CALL_RE.search(
                line
            ) or SUSPICIOUS_HELPER_DEFINITION_RE.search(line)
            if helper_match:
                violations.append(
                    (
                        line_no,
                        f"reintroduction of historical local comparator '{helper_match.group(1)}' "
                        f"is forbidden: use a documented per-anchor "
                        f"family-redundancy filter "
                        f"(e.g. extension_outranks_anchor_base) or delegate to "
                        f"is_better_recovery_key in RecoveryCandidate.hpp",
                    )
                )
    return violations


def main(argv: list[str]) -> int:
    source_root = Path(argv[1]) if len(argv) > 1 else Path.cwd()
    if not source_root.is_dir():
        print(
            f"check_private_comparators: source root not found: {source_root}",
            file=sys.stderr,
        )
        return 2

    failed = False
    for rel_path in INSPECTED_FILES:
        violations = scan_file(source_root, rel_path)
        if violations:
            failed = True
            for line_no, message in violations:
                print(f"{rel_path}:{line_no}: {message}", file=sys.stderr)

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
