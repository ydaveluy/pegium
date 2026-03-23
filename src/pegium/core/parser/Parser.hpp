#pragma once

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/grammar/AbstractRule.hpp>
#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/grammar/Literal.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>
#include <pegium/core/text/TextSnapshot.hpp>
#include <pegium/core/utils/Cancellation.hpp>

namespace pegium::grammar {
struct ParserRule;
}

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

/// Returns whether a diagnostic kind belongs to syntax parsing rather than AST conversion.
[[nodiscard]] constexpr bool
isSyntaxParseDiagnostic(ParseDiagnosticKind kind) noexcept {
  return kind != ParseDiagnosticKind::ConversionError;
}

/// One parser diagnostic anchored to a source range and optional grammar element.
struct ParseDiagnostic {
  /// Diagnostic category.
  ParseDiagnosticKind kind = ParseDiagnosticKind::Deleted;

  /// Parse or recovery position used for ranking and expectation lookup.
  TextOffset offset = 0;

  /// Start offset of the affected source range.
  TextOffset beginOffset = 0;

  /// End offset of the affected source range.
  TextOffset endOffset = 0;

  /// Grammar element that triggered the diagnostic, when known.
  const grammar::AbstractElement *element = nullptr;

  /// Optional human-readable detail, mainly used for conversion failures.
  std::string message;

  /// Returns whether this diagnostic comes from syntax parsing or recovery.
  [[nodiscard]] constexpr bool isSyntax() const noexcept {
    return isSyntaxParseDiagnostic(kind);
  }

private:
  friend std::ostream &operator<<(std::ostream &os,
                                  const ParseDiagnostic &diagnostic) {
    os << "ParseDiagnostic{kind=" << diagnostic.kind
       << ", offset=" << diagnostic.offset
       << ", begin=" << diagnostic.beginOffset
       << ", end=" << diagnostic.endOffset;
    if (diagnostic.element != nullptr) {
      os << ", element=" << *diagnostic.element;
    }
    if (!diagnostic.message.empty()) {
      os << ", message=\"" << diagnostic.message << "\"";
    }
    os << "}";
    return os;
  }
};

/// Heuristics and limits controlling recovery and expectation search.
struct ParseOptions {
  /// Maximum number of codepoints that one attempt may delete consecutively.
  std::uint32_t maxConsecutiveCodepointDeletes = 8;

  /// Initial number of visible tokens included before the failure point.
  std::uint32_t recoveryWindowTokenCount = 8;

  /// Upper bound used when widening the recovery window after failed attempts.
  std::uint32_t maxRecoveryWindowTokenCount = 64;

  /// Maximum number of candidate attempts explored inside one recovery window.
  std::uint32_t maxRecoveryAttempts = 32;

  /// Maximum number of edit operations allowed in a single recovery attempt.
  std::uint32_t maxRecoveryEditsPerAttempt = 8;

  /// Maximum cumulative edit cost allowed in a single recovery attempt.
  std::uint32_t maxRecoveryEditCost = 64;

  /// Maximum number of recovery windows tried for one parse.
  std::uint32_t maxRecoveryWindows = 4;

  /// Enables recovery-aware parsing and expectation tracing after strict failure.
  bool recoveryEnabled = true;
};

/// One expectation alternative as an ordered grammar derivation path.
///
/// The path runs from the outer parsing context to the concrete expected leaf
/// element. For example, a keyword expectation may look like:
/// `[Rule, Assignment, Literal]`.
struct ExpectPath {
  /// Ordered derivation path leading to the expected leaf element.
  ///
  /// Entries are always concrete grammar elements. The vector may be empty for
  /// an unresolved path, but it never stores null pointers.
  std::vector<const grammar::AbstractElement *> elements;

  /// Returns the full path as a lightweight read-only span.
  [[nodiscard]] std::span<const grammar::AbstractElement *const>
  view() const noexcept {
    return elements;
  }

  /// Returns the expected leaf element, or `nullptr` when the path is empty.
  [[nodiscard]] const grammar::AbstractElement *expectedElement() const noexcept {
    return elements.empty() ? nullptr : elements.back();
  }

  /// Returns the expected literal when the leaf is a keyword literal.
  [[nodiscard]] const grammar::Literal *literal() const noexcept {
    const auto *element = expectedElement();
    return element != nullptr &&
                   element->getKind() == grammar::ElementKind::Literal
               ? static_cast<const grammar::Literal *>(element)
               : nullptr;
  }

  /// Returns the expected rule when the leaf itself is a rule element.
  [[nodiscard]] const grammar::AbstractRule *expectedRule() const noexcept {
    const auto *element = expectedElement();
    return element != nullptr && isRuleElementKind(element->getKind())
               ? static_cast<const grammar::AbstractRule *>(element)
               : nullptr;
  }

  /// Returns the expected assignment when the leaf is an assignment.
  [[nodiscard]] const grammar::Assignment *expectedAssignment() const noexcept {
    const auto *element = expectedElement();
    return element != nullptr &&
                   element->getKind() == grammar::ElementKind::Assignment
               ? static_cast<const grammar::Assignment *>(element)
               : nullptr;
  }

  /// Returns the expected assignment when the leaf is a reference assignment.
  [[nodiscard]] const grammar::Assignment *
  expectedReferenceAssignment() const noexcept {
    const auto *assignment = expectedAssignment();
    return assignment != nullptr && assignment->isReference() ? assignment
                                                              : nullptr;
  }

  /// Returns the innermost rule present in the path.
  [[nodiscard]] const grammar::AbstractRule *contextRule() const noexcept {
    for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
      if (isRuleElementKind((*it)->getKind())) {
        return static_cast<const grammar::AbstractRule *>(*it);
      }
    }
    return nullptr;
  }

  /// Returns the innermost assignment present in the path.
  [[nodiscard]] const grammar::Assignment *contextAssignment() const noexcept {
    for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
      if ((*it)->getKind() == grammar::ElementKind::Assignment) {
        return static_cast<const grammar::Assignment *>(*it);
      }
    }
    return nullptr;
  }

  /// Returns whether the path expects a keyword literal.
  [[nodiscard]] bool expectsKeyword() const noexcept { return literal() != nullptr; }

  /// Returns whether the path expects a rule element.
  [[nodiscard]] bool expectsRule() const noexcept {
    return expectedRule() != nullptr;
  }

  /// Returns whether the path expects a reference assignment.
  [[nodiscard]] bool expectsReference() const noexcept {
    return expectedReferenceAssignment() != nullptr;
  }

  /// Returns whether the path has a concrete expected leaf element.
  [[nodiscard]] bool isValid() const noexcept { return expectedElement() != nullptr; }

private:
  [[nodiscard]] static constexpr bool
  isRuleElementKind(grammar::ElementKind kind) noexcept {
    using enum grammar::ElementKind;
    switch (kind) {
    case DataTypeRule:
    case ParserRule:
    case TerminalRule:
    case InfixRule:
      return true;
    default:
      return false;
    }
  }
};

/// Result of expectation tracing at one anchor offset.
struct ExpectResult {
  /// Alternative derivation paths that can continue parsing at `offset`.
  std::vector<ExpectPath> frontier;

  /// Requested anchor offset, clamped to the input size.
  TextOffset offset = 0;

  /// Whether the expectation search reached or explained the requested anchor.
  bool reachedAnchor = false;
};

/// Summary of the last recovery window selected by the parser.
struct RecoveryWindowReport {
  /// Start offset of the recovery window.
  TextOffset beginOffset = 0;

  /// Furthest cursor offset that triggered the window.
  TextOffset maxCursorOffset = 0;

  /// Visible tokens included before the failure point.
  std::uint32_t backwardTokenCount = 0;

  /// Visible tokens allowed while replaying the editable recovery window.
  std::uint32_t forwardTokenCount = 0;
};

/// Aggregate counters describing how recovery behaved for one parse.
struct RecoveryReport {
  /// Whether at least one recovery window was accepted.
  bool hasRecovered = false;

  /// Whether recovery produced a full match of the input.
  bool fullRecovered = false;

  /// Number of accepted recovery windows.
  std::uint32_t recoveryCount = 0;

  /// Total number of recovery windows that were evaluated.
  std::uint32_t recoveryWindowsTried = 0;

  /// Number of strict parse runs executed, including retries.
  std::uint32_t strictParseRuns = 0;

  /// Total number of recovery attempt runs, strict or editable.
  std::uint32_t recoveryAttemptRuns = 0;

  /// Number of edits kept in the selected recovery attempt.
  std::uint32_t recoveryEdits = 0;

  /// Last accepted recovery window, if recovery happened.
  std::optional<RecoveryWindowReport> lastRecoveryWindow;
};

/// Complete parser output for one document parse.
struct ParseResult {
  /// Concrete syntax tree built from the selected parse attempt.
  std::unique_ptr<RootCstNode> cst;

  /// Converted semantic value or AST root when conversion succeeded.
  std::unique_ptr<AstNode> value;

  /// Collected reference handles extracted from the parsed value.
  std::vector<ReferenceHandle> references;

  /// Syntax and conversion diagnostics produced during parsing.
  std::vector<ParseDiagnostic> parseDiagnostics;

  /// Recovery counters for this parse.
  RecoveryReport recoveryReport;

  /// Length consumed by the selected parse attempt.
  TextOffset parsedLength = 0;

  /// Furthest visible-token end reached by the selected parse attempt.
  TextOffset lastVisibleCursorOffset = 0;

  /// Visible-token end closest to the initial syntax failure.
  TextOffset failureVisibleCursorOffset = 0;

  /// Furthest raw cursor offset reached by the selected parse attempt.
  TextOffset maxCursorOffset = 0;

  /// Whether the selected parse attempt matched the full input.
  bool fullMatch = false;
};

/// Abstract parser interface implemented by generated or hand-written parsers.
class Parser {
public:
  virtual ~Parser() noexcept = default;

  /// Returns the entry parser rule of this grammar.
  ///
  /// Pegium uses the entry rule to bootstrap AST reflection before any
  /// document build starts, so parsers are also the source of truth for the
  /// statically known AST type hierarchy.
  [[nodiscard]] virtual const grammar::ParserRule &
  getEntryRule() const noexcept = 0;

  /// Parses one immutable text snapshot and returns the full parse result.
  ///
  /// This is the canonical parser entrypoint used both by workspace documents
  /// and standalone tools.
  [[nodiscard]] virtual ParseResult
  parse(text::TextSnapshot text,
        const utils::CancellationToken &cancelToken = {}) const = 0;

  /// Convenience overload for standalone callers parsing borrowed text.
  ///
  /// The input is copied into a `TextSnapshot` before dispatching to the
  /// virtual snapshot-based parser entrypoint.
  [[nodiscard]] ParseResult
  parse(std::string_view text,
        const utils::CancellationToken &cancelToken = {}) const {
    return parse(text::TextSnapshot::copy(text), cancelToken);
  }

  /// Computes parser expectations at `offset` inside `text`.
  ///
  /// The returned frontier contains the grammar paths that can continue parsing
  /// at the requested anchor, which makes it suitable for diagnostics and
  /// completion providers.
  [[nodiscard]] virtual ExpectResult expect(
      std::string_view text, TextOffset offset,
      const utils::CancellationToken &cancelToken = {}) const = 0;
};

} // namespace pegium::parser
