#pragma once
/// Parser terminal matching a fixed literal.
#include <algorithm>
#include <cstdint>
#include <optional>
#include <pegium/core/grammar/Literal.hpp>
#include <pegium/core/parser/ExpectContext.hpp>
#include <pegium/core/parser/LiteralFuzzyMatcher.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/ParseExpression.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>
#include <pegium/core/parser/RecoveryTrace.hpp>
#include <pegium/core/parser/TerminalRecoverySupport.hpp>
#include <pegium/core/parser/TextUtils.hpp>
#include <ranges>
#include <string>

namespace pegium::parser {

template <Expression... Elements> struct Group;
template <Expression... Elements> struct GroupWithSkipper;

template <auto literal, bool case_sensitive = true>
struct Literal final : grammar::Literal {
  static_assert(std::ranges::none_of(literal, [](char c) { return c == '\0'; }),
                "Literal cannot contain '\\0'.");
  static constexpr bool nullable = literal.empty();
  static constexpr bool isFailureSafe = true;
  static constexpr bool isWordLike = []() constexpr noexcept {
    constexpr std::string_view literalValue{literal.begin(), literal.size()};
    return detail::is_word_like_terminal(literalValue);
  }();
  using type = std::string_view;
  using grammar::Literal::getValue;
  std::string_view getValue() const noexcept override {
    return {literal.begin(), literal.size()};
  }
  std::string_view getValue(const CstNodeView &node) const noexcept override {
    if (!node.isRecovered()) [[likely]] {
      return node.getText();
    }
    return getValue();
  }
  std::string_view getRawValue(const CstNodeView &node) const noexcept {
    return getValue(node);
  }
  bool isCaseSensitive() const noexcept override { return case_sensitive; }

private:
  friend struct detail::ParseAccess;
  friend struct detail::ProbeAccess;
  template <Expression... Elements> friend struct Group;
  template <Expression... Elements> friend struct GroupWithSkipper;

  using LocalRecoveryChoice = detail::TerminalRecoveryCandidate;

  template <EditableParseModeContext Context>
  [[nodiscard]] static const char *fuzzyInputEnd(const Context &ctx,
                                                 const char *cursor,
                                                 const char *limit) noexcept {
    if (cursor == nullptr || cursor >= limit) {
      return cursor;
    }

    if constexpr (requires { ctx.skip_without_builder(cursor); }) {
      const char *it = cursor;
      while (it < limit) {
        if (ctx.skip_without_builder(it) > it) {
          return it;
        }
        ++it;
      }
      return limit;
    } else {
      constexpr std::size_t kExtraWindow = 4u;
      const auto span = static_cast<std::size_t>(limit - cursor);
      return cursor + std::min(span, static_cast<std::size_t>(literal.size() +
                                                              kExtraWindow));
    }
  }

  template <EditableParseModeContext Context>
  [[nodiscard]] static std::string_view
  fuzzyInputView(const Context &ctx, const char *cursor,
                 const char *limit) noexcept {
    const auto *viewEnd = fuzzyInputEnd(ctx, cursor, limit);
    return {cursor, static_cast<std::size_t>(viewEnd - cursor)};
  }

  template <EditableParseModeContext Context>
  [[nodiscard]] static constexpr const char *
  fuzzyInputLimit(const Context &ctx) noexcept {
    if constexpr (RecoveryParseModeContext<Context>) {
      return ctx.end;
    } else {
      return ctx.anchor;
    }
  }

  template <EditableParseModeContext Context>
  [[nodiscard]] detail::LiteralFuzzyCandidates findReplaceCandidates(
      Context &ctx, const char *cursorStart,
      detail::TerminalRecoveryFacts terminalRecoveryFacts = {}) const noexcept {
    detail::LiteralFuzzyCandidates filteredCandidates;
    if constexpr (!allow_replace) {
      (void)ctx;
      (void)cursorStart;
      return filteredCandidates;
    } else {
      constexpr auto lexicalProfile = detail::classify_literal_recovery_profile(
          std::string_view{literal.begin(), literal.size()});
      return detail::collect_literal_replace_candidates(
          ctx, cursorStart, getValue(), case_sensitive, lexicalProfile,
          terminalRecoveryFacts);
    }
  }

  template <EditableParseModeContext Context, typename HasWordBoundaryViolation>
  [[nodiscard]] LocalRecoveryChoice selectLocalRecoveryChoice(
      Context &ctx, const char *cursorStart, const char *matchedEnd,
      const detail::LiteralFuzzyCandidates &fuzzyCandidates,
      detail::TerminalRecoveryFacts terminalRecoveryFacts,
      HasWordBoundaryViolation &&hasWordBoundaryViolation) const {
    constexpr auto lexicalRecoveryProfile =
        detail::classify_literal_recovery_profile(
            std::string_view{literal.begin(), literal.size()});
    LocalRecoveryChoice bestChoice;
    auto considerChoice = [&bestChoice](const LocalRecoveryChoice &choice) {
      if (detail::is_better_normalized_recovery_order_key(
              detail::terminal_recovery_order_key(choice),
              detail::terminal_recovery_order_key(bestChoice),
              detail::RecoveryOrderProfile::Terminal)) {
        bestChoice = choice;
      }
    };

    if constexpr (allow_matched_insert) {
      if (matchedEnd != nullptr && hasWordBoundaryViolation(matchedEnd)) {
        considerChoice(detail::evaluate_insert_synthetic_gap_terminal_candidate(
            ctx, cursorStart, matchedEnd, this, 3u, 3u));
      }
    }

    if constexpr (allow_replace) {
      for (const auto &fuzzyCandidate : fuzzyCandidates) {
        const char *const fuzzyEnd = cursorStart + fuzzyCandidate.consumed;
        if (detail::can_apply_recovery_match(ctx, fuzzyEnd) &&
            !hasWordBoundaryViolation(fuzzyEnd)) {
          considerChoice(detail::evaluate_replace_leaf_terminal_candidate(
              ctx, cursorStart, fuzzyEnd, this, fuzzyCandidate.cost,
              fuzzyCandidate.distance, fuzzyCandidate.substitutionCount,
              fuzzyCandidate.operationCount,
              detail::terminal_anchor_quality(
                  terminalRecoveryFacts.triviaGap)));
        }
      }
    }

    return detail::complete_terminal_recovery_choice(
        ctx, cursorStart, this, terminalRecoveryFacts, lexicalRecoveryProfile,
        allow_insert, bestChoice,
        [this, &ctx, &hasWordBoundaryViolation,
         cursorStart](const char *scanStart) noexcept -> const char * {
          if (!detail::allows_extended_terminal_delete_scan_match(
                  ctx, cursorStart, scanStart)) {
            return nullptr;
          }
          const char *const scanEnd = terminal(scanStart);
          if (scanEnd == nullptr ||
              !detail::can_apply_recovery_match(ctx, scanEnd) ||
              hasWordBoundaryViolation(scanEnd)) {
            return nullptr;
          }
          return scanEnd;
        },
        [this, &ctx, cursorStart](const char *scanEnd) {
          ctx.leaf(cursorStart, scanEnd, this, false, true);
        });
  }

  template <EditableParseModeContext Context, typename HasWordBoundaryViolation>
  bool applyLocalRecoveryChoice(
      Context &ctx, const LocalRecoveryChoice &choice, const char *cursorStart,
      const char *matchedEnd,
      detail::TerminalRecoveryFacts terminalRecoveryFacts,
      HasWordBoundaryViolation &&hasWordBoundaryViolation) const {
    return detail::apply_terminal_recovery_choice(
        ctx, choice, this,
        [this, &ctx, matchedEnd, &hasWordBoundaryViolation]() {
          if (matchedEnd == nullptr || !hasWordBoundaryViolation(matchedEnd) ||
              !detail::apply_insert_synthetic_gap_and_match_recovery_edit(
                  ctx, matchedEnd, this, "Expecting separator")) {
            return false;
          }
          if constexpr (RecoveryParseModeContext<Context>) {
            PEGIUM_RECOVERY_TRACE(
                "[literal rule] split word boundary for '", getValue(), "' at ",
                static_cast<TextOffset>(matchedEnd - ctx.begin));
          }
          return true;
        },
        [this, &ctx, cursorStart, &choice, &hasWordBoundaryViolation]() {
          const char *const fuzzyEnd = cursorStart + choice.consumed;
          if (!detail::can_apply_recovery_match(ctx, fuzzyEnd) ||
              hasWordBoundaryViolation(fuzzyEnd) ||
              !detail::apply_replace_leaf_recovery_edit(
                  ctx, fuzzyEnd, this, choice.cost.budgetCost)) {
            return false;
          }
          if constexpr (RecoveryParseModeContext<Context>) {
            PEGIUM_RECOVERY_TRACE("[literal rule] fuzzy match '", getValue(),
                                  "' at ", ctx.cursorOffset(),
                                  " distance=", choice.distance,
                                  " cost=", choice.cost.budgetCost,
                                  " rank=", choice.cost.primaryRankCost);
          }
          return true;
        },
        [this, &ctx]() {
          if constexpr (RecoveryParseModeContext<Context>) {
            PEGIUM_RECOVERY_TRACE("[literal rule] inserted '", getValue(),
                                  "' at ", ctx.cursorOffset());
          }
        },
        [this, &ctx, &hasWordBoundaryViolation, cursorStart,
         terminalRecoveryFacts]() {
          return detail::apply_delete_scan_terminal_candidate(
              ctx,
              [this, &ctx, &hasWordBoundaryViolation,
               cursorStart](const char *scanStart) noexcept -> const char * {
                if (!detail::allows_extended_terminal_delete_scan_match(
                        ctx, cursorStart, scanStart)) {
                  return nullptr;
                }
                const char *const scanEnd = terminal(scanStart);
                if (scanEnd == nullptr ||
                    !detail::can_apply_recovery_match(ctx, scanEnd) ||
                    hasWordBoundaryViolation(scanEnd)) {
                  return nullptr;
                }
                return scanEnd;
              },
              [this, &ctx, cursorStart](const char *scanEnd) {
                if constexpr (RecoveryParseModeContext<Context>) {
                  PEGIUM_RECOVERY_TRACE("[literal rule] delete-scan match '",
                                        getValue(), "' at ",
                                        ctx.cursorOffset());
                }
                ctx.leaf(cursorStart, scanEnd, this, false, true);
              },
              terminalRecoveryFacts, lexicalRecoveryProfile);
        });
  }

  bool probe_impl(const ParseContext &ctx) const noexcept {
    const char *const matchEnd = terminal(ctx.cursor());
    if (matchEnd == nullptr) {
      return false;
    }
    return !detail::literal_has_word_boundary_violation(getValue(), matchEnd);
  }

  template <EditableParseModeContext Context>
  bool parse_terminal_recovery_impl(
      Context &ctx,
      const detail::TerminalRecoveryFacts &terminalRecoveryFacts) const {
    const char *const cursorStart = ctx.cursor();
    const bool hasHadEdits = []<typename Ctx>(const Ctx &currentCtx) constexpr
                                 noexcept {
                                   if constexpr (requires {
                                                   currentCtx.hasHadEdits();
                                                 }) {
                                     return currentCtx.hasHadEdits();
                                   }
                                   return false;
                                 }(ctx);
    const char *const matchedEnd = terminal(cursorStart);
    constexpr auto lexicalRecoveryProfile =
        detail::classify_literal_recovery_profile(
            std::string_view{literal.begin(), literal.size()});
    const auto hasWordBoundaryViolation = [this](const char *end) noexcept {
      return detail::literal_has_word_boundary_violation(getValue(), end);
    };

    if constexpr (RecoveryParseModeContext<Context>) {
      if (matchedEnd != nullptr && !hasWordBoundaryViolation(matchedEnd)) {
        PEGIUM_RECOVERY_TRACE("[literal rule] direct match '", getValue(),
                              "' at ", ctx.cursorOffset());
        ctx.leaf(matchedEnd, this);
        return true;
      }
      if (!ctx.canEdit()) {
        return false;
      }
      const auto fuzzyCandidates =
          (hasHadEdits &&
           !detail::allows_fuzzy_replace_after_prior_edits(
               lexicalRecoveryProfile))
              ? detail::LiteralFuzzyCandidates{}
              : findReplaceCandidates(ctx, cursorStart, terminalRecoveryFacts);
      if (const auto bestChoice = selectLocalRecoveryChoice(
              ctx, cursorStart, matchedEnd, fuzzyCandidates,
              terminalRecoveryFacts, hasWordBoundaryViolation);
          applyLocalRecoveryChoice(ctx, bestChoice, cursorStart, matchedEnd,
                                   terminalRecoveryFacts,
                                   hasWordBoundaryViolation)) {
        return true;
      }

      PEGIUM_RECOVERY_TRACE("[literal rule] fail '", getValue(), "' at ",
                            ctx.cursorOffset());
      return false;
    } else {
      if (ctx.reachedAnchor()) {
        ctx.addKeyword(this);
        return true;
      }

      if (matchedEnd != nullptr && ctx.canTraverseUntil(matchedEnd) &&
          !hasWordBoundaryViolation(matchedEnd)) {
        ctx.leaf(matchedEnd, this);
        return true;
      }
      if (const auto anchorDistance =
              static_cast<std::size_t>(ctx.anchor - cursorStart);
          cursorStart < ctx.anchor && anchorDistance <= literal.size()) {
        bool prefixMatches = true;
        for (std::size_t index = 0; index < anchorDistance; ++index) {
          const auto lhs = static_cast<unsigned char>(cursorStart[index]);
          const auto rhs = static_cast<unsigned char>(literal[index]);
          if constexpr (case_sensitive) {
            if (lhs != rhs) {
              prefixMatches = false;
              break;
            }
          } else if (tolower(lhs) != rhs) {
            prefixMatches = false;
            break;
          }
        }
        if (prefixMatches) {
          ctx.addKeyword(this);
          return true;
        }
      }

      if (!ctx.canEdit()) {
        return false;
      }

      const auto fuzzyCandidates =
          (hasHadEdits &&
           !detail::allows_fuzzy_replace_after_prior_edits(
               lexicalRecoveryProfile))
              ? detail::LiteralFuzzyCandidates{}
              : findReplaceCandidates(ctx, cursorStart, terminalRecoveryFacts);
      const auto bestChoice = selectLocalRecoveryChoice(
          ctx, cursorStart, matchedEnd, fuzzyCandidates, terminalRecoveryFacts,
          hasWordBoundaryViolation);
      return applyLocalRecoveryChoice(ctx, bestChoice, cursorStart, matchedEnd,
                                      terminalRecoveryFacts,
                                      hasWordBoundaryViolation);
    }
  }

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    if constexpr (StrictParseModeContext<Context>) {
      const char *const matchedEnd = terminal(ctx.cursor());
      const auto hasWordBoundaryViolation = [this](const char *end) noexcept {
        return detail::literal_has_word_boundary_violation(getValue(), end);
      };
      if (matchedEnd == nullptr || hasWordBoundaryViolation(matchedEnd)) {
        return false;
      }
      PEGIUM_RECOVERY_TRACE("[literal rule] direct match '", getValue(),
                            "' at ", ctx.cursorOffset());
      ctx.leaf(matchedEnd, this);
      return true;
    } else {
      return parse_terminal_recovery_impl(ctx, {});
    }
  }

public:
  bool probeRecoverable(RecoveryContext &ctx) const noexcept {
    constexpr auto lexicalRecoveryProfile =
        detail::classify_literal_recovery_profile(
            std::string_view{literal.begin(), literal.size()});
    const char *const cursorStart = ctx.cursor();
    if constexpr (allow_matched_insert) {
      const auto hasWordBoundaryViolation = [](const char *end) noexcept {
        return detail::literal_has_word_boundary_violation(literalValue, end);
      };
      if (const char *const matchedEnd = terminal(cursorStart);
          matchedEnd != nullptr && hasWordBoundaryViolation(matchedEnd)) {
        return true;
      }
    }
    if (ctx.hasHadEdits() &&
        !detail::allows_fuzzy_replace_after_prior_edits(
            lexicalRecoveryProfile)) {
      return false;
    }
    if constexpr (allow_replace) {
      const auto fuzzyCandidates = findReplaceCandidates(ctx, cursorStart);
      if (!fuzzyCandidates.empty()) {
        return true;
      }
    }
    return detail::probe_nearby_delete_scan_match(
        ctx,
        [this](const char *scanCursor) noexcept {
          return terminal(scanCursor);
        },
        {}, lexicalRecoveryProfile);
  }

  bool probeRecoverableAtEntry(RecoveryContext &ctx) const noexcept {
    constexpr auto lexicalRecoveryProfile =
        detail::classify_literal_recovery_profile(
            std::string_view{literal.begin(), literal.size()});
    if constexpr (allow_insert) {
      if (detail::allows_terminal_entry_probe(ctx, lexicalRecoveryProfile)) {
        return true;
      }
    }
    if (ctx.hasHadEdits() &&
        !detail::allows_fuzzy_replace_after_prior_edits(
            lexicalRecoveryProfile)) {
      return false;
    }
    if (!detail::allows_compact_local_gap_terminal_recovery(ctx)) {
      return false;
    }
    const char *const cursorStart = ctx.cursor();
    if constexpr (allow_matched_insert) {
      const auto hasWordBoundaryViolation = [](const char *end) noexcept {
        return detail::literal_has_word_boundary_violation(literalValue, end);
      };
      if (const char *const matchedEnd = terminal(cursorStart);
          matchedEnd != nullptr && hasWordBoundaryViolation(matchedEnd)) {
        return true;
      }
    }
    if constexpr (allow_replace) {
      const auto fuzzyCandidates = findReplaceCandidates(ctx, cursorStart);
      return std::ranges::any_of(
          fuzzyCandidates, [](const auto &candidate) constexpr noexcept {
            return detail::allows_entry_probe_fuzzy_candidate(candidate);
          });
    }
    return false;
  }

  constexpr const char *terminal(const char *begin) const noexcept {
    for (std::size_t charIndex = 0; charIndex < literal.size(); ++charIndex) {
      if constexpr (case_sensitive) {
        if (begin[charIndex] != literal[charIndex]) {
          return nullptr;
        }
      } else {
        if (tolower(begin[charIndex]) != literal[charIndex]) {
          return nullptr;
        }
      }
    }
    return begin + literal.size();
  }

  constexpr const char *terminal(const std::string &text) const noexcept {
    return terminal(text.c_str());
  }

  /// Create an insensitive Literal
  /// @return the insensitive Literal
  constexpr auto i() const noexcept {
    return Literal<toLower(), isCaseSensitive(literal)>{};
  }

  constexpr bool isNullable() const noexcept override { return nullable; }

private:
  static constexpr std::string_view literalValue{literal.data(),
                                                 literal.size()};
  // Keep generic recovery able to synthesize only compact literals and avoid
  // inventing long multi-character tokens.
  static constexpr auto lexicalRecoveryProfile =
      detail::classify_literal_recovery_profile(literalValue);
  static constexpr bool allow_insert = lexicalRecoveryProfile.allowsInsert();
  static constexpr bool allow_matched_insert =
      lexicalRecoveryProfile.allowsMatchedInsert();
  static constexpr bool allow_replace = lexicalRecoveryProfile.allowsReplace();

  static constexpr auto toLower() {
    decltype(literal) newLiteral;
    std::ranges::transform(literal, newLiteral.begin(),
                           [](char c) { return tolower(c); });
    return newLiteral;
  }

  static constexpr bool isCaseSensitive(auto lit) {
    return std::ranges::none_of(lit, [](char c) {
      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    });
  }
};

template <typename T> struct IsLiteralImpl : std::false_type {};
template <auto literal, bool case_sensitive>
struct IsLiteralImpl<Literal<literal, case_sensitive>> : std::true_type {};

template <auto literal, bool case_sensitive>
struct detail::IsTerminalAtom<Literal<literal, case_sensitive>>
    : std::true_type {};

template <typename T>
concept IsLiteral = IsLiteralImpl<T>::value;

} // namespace pegium::parser
