#pragma once

/// `TerminalShape`: the closed lexical profile a terminal recovery
/// step reads to admit `Replace`, `Insert`, or `DeleteScan`
/// candidates.
///
/// The shape names six concrete properties of the terminal that an
/// admission rule can reason about:
///
///   - `hasCanonicalText`        the terminal has a canonical
///                               literal value (e.g. a keyword);
///                               required for `Replace`.
///   - `canonicalTextLength`     length in bytes of the canonical
///                               text; feeds the fuzzy lookahead
///                               window calculation.
///   - `singleCodepoint`         true iff the canonical text is a
///                               single codepoint; single-codepoint
///                               replacements must stay conservative.
///   - `startsLikeWord`          the canonical text begins with a
///                               word-like codepoint; used to
///                               detect leading-edge boundary
///                               violations during fuzzy match.
///   - `endsLikeWord`            the canonical text ends with a
///                               word-like codepoint; used to
///                               detect trailing-edge boundary
///                               violations.
///   - `boundarySensitive`       true iff the terminal must respect
///                               word boundaries on either side
///                               (typically derived from the two
///                               flags above).
///
/// Strict-path discipline: this header is recovery-side. It must
/// never be constructed on the strict-only nominal path.

#include <cstdint>
#include <string_view>
#include <type_traits>

namespace pegium::parser::detail {

inline constexpr std::uint32_t kMaxScopedSyntheticInsertCanonicalTextLength = 2;

/// The closed lexical shape of a terminal. Six fields. Adding a
/// field requires removing or merging an existing one (the density
/// ceiling rule applies to local types too).
struct TerminalShape {
  bool hasCanonicalText = false;
  std::uint32_t canonicalTextLength = 0;
  bool singleCodepoint = false;
  bool startsLikeWord = false;
  bool endsLikeWord = false;
  bool boundarySensitive = false;

  /// Replace requires multi-codepoint canonical text —
  /// single-codepoint replacements stay conservative and go through
  /// stricter paths. Mirrors the legacy
  /// `LexicalRecoveryProfile::allowsReplace` so helpers can swap
  /// parameter type without changing their body.
  [[nodiscard]] constexpr bool allowsReplace() const noexcept {
    return hasCanonicalText && !singleCodepoint;
  }

  /// Insert requires a single-codepoint canonical text. Wider compact
  /// punctuation can be admitted by a caller-owned scoped rule, but it is
  /// not part of the terminal's global insert capability.
  [[nodiscard]] constexpr bool allowsInsert() const noexcept {
    return hasCanonicalText && singleCodepoint;
  }

  [[nodiscard]] friend bool
  operator==(const TerminalShape &a, const TerminalShape &b) noexcept = default;
};

static_assert(std::is_trivially_copyable_v<TerminalShape>);
static_assert(sizeof(TerminalShape) <= 12);

/// Builds a `TerminalShape` from a literal value. Conservative on
/// empty input (returns the all-default shape). The `is_word_like`
/// classification is delegated to the caller because it depends on
/// the codepoint-classification predicates already in the parser.
[[nodiscard]] constexpr TerminalShape
make_terminal_shape_from_literal(std::string_view canonicalText,
                                  bool startsLikeWord,
                                  bool endsLikeWord) noexcept {
  if (canonicalText.empty()) {
    return TerminalShape{};
  }
  TerminalShape shape;
  shape.hasCanonicalText = true;
  shape.canonicalTextLength =
      static_cast<std::uint32_t>(canonicalText.size());
  shape.singleCodepoint = canonicalText.size() == 1U;
  shape.startsLikeWord = startsLikeWord;
  shape.endsLikeWord = endsLikeWord;
  shape.boundarySensitive = startsLikeWord || endsLikeWord;
  return shape;
}

/// Closed legality facts the terminal recovery predicates consume.
///
/// Disambiguated from the legacy `TerminalRecoveryFacts` in
/// `TerminalRecoverySupport.hpp` so both can coexist during the
/// migration. The legacy struct carries the trivia/insertion
/// scaffolding the existing helpers consume; this struct carries the
/// closed legality vocabulary the `is_terminal_*_legal` predicates
/// consume.
struct TerminalLegalityFacts {
  /// True iff the recovery budget allows another fuzzy `Replace`
  /// candidate at the current position (`maxRecoveryEditCost` minus
  /// `currentEditCost` covers the replacement cost).
  bool budgetAllowsReplace = false;

  /// True iff the recovery budget allows a `DeleteScan` at the
  /// current position.
  bool budgetAllowsDeleteScan = false;

  /// True iff a tail or follow primary accepts immediately after
  /// the candidate insertion site. Required for `Insert`.
  bool tailOrFollowContinuationAvailable = false;

  /// True iff a strict match of the target terminal accepts after
  /// the scan, OR the expected follow accepts. Required for
  /// `DeleteScan`: the scan must commit to a strict terminal match
  /// or to the expected follow.
  bool strictTerminalOrFollowAfterScan = false;

  /// True iff a leading-edge word boundary would be violated by a
  /// fuzzy match (the candidate would attach to an adjacent word).
  /// Disqualifies `Replace` when the shape is `boundarySensitive`.
  bool fuzzyMatchViolatesLeadingBoundary = false;

  /// True iff a trailing-edge word boundary would be violated by a
  /// fuzzy match.
  bool fuzzyMatchViolatesTrailingBoundary = false;

  [[nodiscard]] friend bool
  operator==(const TerminalLegalityFacts &a,
             const TerminalLegalityFacts &b) noexcept = default;
};

static_assert(std::is_trivially_copyable_v<TerminalLegalityFacts>);
static_assert(sizeof(TerminalLegalityFacts) <= 8);

/// `Replace` requires a canonical text, a non-single-codepoint shape
/// (single-codepoint replacements stay conservative — they go through
/// stricter paths, not generic `Replace`), an available budget, and
/// no boundary violation when the shape is boundary-sensitive.
[[nodiscard]] constexpr bool
is_terminal_replace_legal(const TerminalShape &shape,
                           const TerminalLegalityFacts &facts) noexcept {
  if (!shape.hasCanonicalText || shape.singleCodepoint) {
    return false;
  }
  if (!facts.budgetAllowsReplace) {
    return false;
  }
  if (shape.boundarySensitive &&
      (facts.fuzzyMatchViolatesLeadingBoundary ||
       facts.fuzzyMatchViolatesTrailingBoundary)) {
    return false;
  }
  return true;
}

/// `Insert` requires an immediate tail or follow continuation and a
/// terminal shape with global insert capability.
[[nodiscard]] constexpr bool
is_terminal_insert_legal(const TerminalShape &shape,
                          const TerminalLegalityFacts &facts) noexcept {
  if (!facts.tailOrFollowContinuationAvailable) {
    return false;
  }
  if (!shape.allowsInsert()) {
    return false;
  }
  return true;
}

/// `DeleteScan` requires a strict match of the terminal or the
/// expected follow after the scan, plus an available delete budget.
[[nodiscard]] constexpr bool
is_terminal_delete_scan_legal(
    const TerminalShape & /*shape*/,
    const TerminalLegalityFacts &facts) noexcept {
  return facts.strictTerminalOrFollowAfterScan &&
         facts.budgetAllowsDeleteScan;
}

/// Computes the fuzzy lookahead window:
///
///     maxLookahead = canonicalTextLength + affordableDeleteSpan
///
/// `affordableDeleteSpan` is supplied by the caller (typically the
/// remaining recovery budget translated to a byte span). Returning
/// 0 when the shape has no canonical text reflects the rule:
/// `Replace` is not admissible without a canonical text, so a
/// fuzzy window of 0 is correct.
[[nodiscard]] constexpr std::uint32_t
compute_terminal_max_lookahead(const TerminalShape &shape,
                                std::uint32_t affordableDeleteSpan) noexcept {
  if (!shape.hasCanonicalText) {
    return 0;
  }
  return shape.canonicalTextLength + affordableDeleteSpan;
}

} // namespace pegium::parser::detail
