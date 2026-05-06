#pragma once

/// Closed enumeration of parser diagnostic categories.
///
/// Extracted to a minimal header so layers that only read the kind
/// (recovery, terminal-shape, candidate envelopes) do not transitively
/// pull `Parser.hpp` and its 200+ public API types.

#include <iosfwd>

namespace pegium::parser {

/// Category of diagnostic emitted by parsing, recovery, or value conversion.
enum class ParseDiagnosticKind {
  /// Recovery inserted a missing construct without consuming source text.
  Inserted,
  /// Recovery skipped a portion of source text.
  Deleted,
  /// Recovery consumed source text as another expected construct.
  Replaced,
  /// Parsing stopped at end of input while more content was still expected.
  Incomplete,
  /// Parsing resumed after a previous syntax issue.
  Recovered,
  /// CST parsing succeeded but value conversion failed afterwards.
  ConversionError,
};

/// Writes a human-readable name for a diagnostic kind.
std::ostream &operator<<(std::ostream &os, ParseDiagnosticKind kind);

/// Returns whether a diagnostic kind belongs to syntax parsing rather than
/// AST conversion.
[[nodiscard]] constexpr bool
isSyntaxParseDiagnostic(ParseDiagnosticKind kind) noexcept {
  return kind != ParseDiagnosticKind::ConversionError;
}

} // namespace pegium::parser
