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
#include <pegium/core/parser/ParseDiagnosticKind.hpp>
#include <pegium/core/parser/RecoveryConstants.hpp>
#include <pegium/core/syntax-tree/AstArena.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>
#include <pegium/core/text/TextSnapshot.hpp>
#include <pegium/core/utils/Cancellation.hpp>

namespace pegium::grammar {
struct ParserRule;
}

namespace pegium::parser {
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

/// Coalesces adjacent recovery diagnostics into stable parser-facing output.
///
/// This keeps delete runs contiguous and merges same-offset inserted
/// expectations so downstream consumers do not need to reconstruct recovery
/// edit structure on their own.
[[nodiscard]] std::vector<ParseDiagnostic>
normalizeParseDiagnostics(std::span<const ParseDiagnostic> diagnostics);

/// Heuristics and limits controlling recovery and expectation search.
struct ParseOptions {
  /// Preferred limit for one contiguous delete run during normal recovery.
  ///
  /// Delete-scan recovery may temporarily continue past this limit when the
  /// normal budget still allows more deletions and no earlier match was found.
  std::uint32_t maxConsecutiveCodepointDeletes =
      kDefaultMaxConsecutiveCodepointDeletes;

  /// Initial number of visible tokens included before the failure point.
  std::uint32_t recoveryWindowTokenCount = 16;

  /// Upper bound for the in-window forward-widening retry.
  ///
  /// When the first attempt at a failure site is rejected as non-credible,
  /// the recovery pass reruns the local pipeline with a larger forward
  /// window (up to this cap) before advancing to the next site. The
  /// cap must be at least `recoveryWindowTokenCount`.
  std::uint32_t maxRecoveryWindowTokenCount = 64;

  /// Maximum number of candidate attempts explored inside one recovery window.
  std::uint32_t maxRecoveryAttempts = 64;

  /// Maximum number of edit operations allowed while replaying one active
  /// recovery window.
  std::uint32_t maxRecoveryEditsPerAttempt = 64;

  /// Preferred cumulative edit-cost budget for one recovery attempt.
  ///
  /// Some delete-scan retries may briefly exceed this while searching for a
  /// later resynchronization point. Such attempts are only considered
  /// selectable when they keep a stable prefix before the first edit.
  std::uint32_t maxRecoveryEditCost = 256;

  /// Hard global safety valve on total recovery attempt runs (budgeted and
  /// validation-only). Prevents pathological inputs or fuzzer cases from
  /// driving unbounded recovery work.
  std::uint32_t maxTotalRecoveryAttemptRuns = 1024;

  /// Hard cap on the cumulative number of `ParserRule` recovery entries
  /// allowed within one recovery window. Bounds the speculative search tree
  /// when a pathological grammar shape (e.g. unclosed nested call
  /// expressions) would otherwise cause `evaluate_editable_recovery_candidate`
  /// to explore exponentially many branches. Reaching the cap makes the
  /// inner recovery fail fast so the outer driver falls back to a less
  /// ambitious candidate.
  std::uint32_t maxRecoveryRuleEntries = 12000;

  /// Number of strict visible leaves that must parse after a recovery edit
  /// before the edit is considered stable.
  std::uint32_t recoveryStabilityTokenCount = 2;

  /// Maximum bytes a `Repetition` may skip when no normal recovery plan can
  /// bridge the current iteration. Acts as the last-resort panic-mode budget:
  /// the iteration scans forward up to this many codepoints looking for a
  /// clean restart of its element, emitting one fused `Delete` on success.
  /// Set to `0` to disable the panic-mode candidate entirely.
  std::uint32_t maxResyncSkipBytes = 4096;

  /// Enables recovery-aware parsing and expectation tracing after strict failure.
  bool recoveryEnabled = true;

  /// Test-only / diagnostic options. Nested under a dedicated
  /// substruct so production paths can ignore them and the test
  /// harness has an explicit address for them. These MUST NOT
  /// change the chosen recovery candidate; their only role is to
  /// give tests access to internal optimisations (e.g. caches).
  ///
  /// Production callers should leave this at default. The substruct
  /// is public because C++ has no friend-namespace mechanism that
  /// would let a test header mutate a private field cleanly; the
  /// `Diagnostics` name + documentation are the contract that
  /// production code does not touch them.
  struct Diagnostics {
    /// Disables the `ChoiceRecoverCache` used to memoize
    /// `OrderedChoice` recovery attempts. The cache is purely an
    /// optimisation; the cache-neutrality harness flips this to
    /// prove that disabling the cache never changes the chosen
    /// candidate. Production code MUST leave this at `false`.
    bool recoveryCacheDisabled = false;
  };
  Diagnostics diagnostics;
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

  /// OrderedChoice recovery cache hits during this parse.
  std::uint64_t choiceRecoverCacheHits = 0;

  /// OrderedChoice recovery cache misses during this parse.
  std::uint64_t choiceRecoverCacheMisses = 0;

  /// Last accepted recovery window, if recovery happened.
  std::optional<RecoveryWindowReport> lastRecoveryWindow;
};

/// Complete parser output for one document parse.
struct ParseResult {
  /// Arena that owns every AstNode reachable from `value`.
  ///
  /// Declared BEFORE `cst` so that:
  /// - the implicit move assignment destroys the previous `astArena` first,
  ///   while the previous `cst` (and its pool) is still alive;
  /// - the destructor body explicitly resets `astArena` first to satisfy the
  ///   same ordering against the implicit member destruction.
  /// AST nodes are allocated from the CST root's monotonic buffer pool, so
  /// the arena MUST be torn down before the CST.
  std::unique_ptr<AstArena> astArena;

  /// Concrete syntax tree built from the selected parse attempt.
  std::unique_ptr<RootCstNode> cst;

  /// AST root produced by conversion, or nullptr when conversion failed.
  ///
  /// Non-owning: the root and every reachable child are stored inside
  /// `astArena`.
  AstNode *value = nullptr;

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

  ParseResult() = default;
  ParseResult(const ParseResult &) = delete;
  ParseResult &operator=(const ParseResult &) = delete;
  ParseResult(ParseResult &&) = default;
  ParseResult &operator=(ParseResult &&) = default;

  /// Reset `astArena` first so AST node destructors run while the CST pool
  /// is still alive. The implicit member destruction afterwards releases
  /// `cst` (and its pool) and finds an already-empty `astArena`.
  ~ParseResult() noexcept { astArena.reset(); }
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
