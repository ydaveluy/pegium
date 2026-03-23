#pragma once

/// Ranking heuristics for comparing alternative parse attempts.

namespace pegium::parser::detail {

template <typename Attempt>
[[nodiscard]] constexpr bool
is_better_parse_attempt(const Attempt &lhs, const Attempt &rhs) noexcept {
  if (lhs.entryRuleMatched != rhs.entryRuleMatched) {
    return lhs.entryRuleMatched && !rhs.entryRuleMatched;
  }
  if (lhs.fullMatch != rhs.fullMatch) {
    return lhs.fullMatch && !rhs.fullMatch;
  }
  if (lhs.fullMatch && rhs.fullMatch) {
    if constexpr (requires {
                    lhs.hasDiagnostics;
                    rhs.hasDiagnostics;
                  }) {
      if (lhs.hasDiagnostics != rhs.hasDiagnostics) {
        return !lhs.hasDiagnostics && rhs.hasDiagnostics;
      }
    }
    if (lhs.editCost != rhs.editCost) {
      return lhs.editCost < rhs.editCost;
    }
    if constexpr (requires {
                    lhs.hasDiagnostics;
                    rhs.hasDiagnostics;
                    lhs.firstDiagnosticOffset;
                    rhs.firstDiagnosticOffset;
                  }) {
      if (lhs.hasDiagnostics && rhs.hasDiagnostics &&
          lhs.firstDiagnosticOffset != rhs.firstDiagnosticOffset) {
        return lhs.firstDiagnosticOffset > rhs.firstDiagnosticOffset;
      }
    }
  }
  if (lhs.parsedLength != rhs.parsedLength) {
    return lhs.parsedLength > rhs.parsedLength;
  }
  if (lhs.maxCursorOffset != rhs.maxCursorOffset) {
    return lhs.maxCursorOffset > rhs.maxCursorOffset;
  }
  if (lhs.editCost != rhs.editCost) {
    return lhs.editCost < rhs.editCost;
  }
  if (lhs.parseDiagnostics.size() != rhs.parseDiagnostics.size()) {
    return lhs.parseDiagnostics.size() < rhs.parseDiagnostics.size();
  }
  return false;
}

} // namespace pegium::parser::detail
