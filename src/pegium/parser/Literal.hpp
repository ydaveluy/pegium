#pragma once
#include <algorithm>
#include <cstdint>
#include <pegium/grammar/Literal.hpp>
#include <pegium/parser/ExpectContext.hpp>
#include <pegium/parser/LiteralFuzzyMatcher.hpp>
#include <pegium/parser/ParseMode.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/RecoveryCandidate.hpp>
#include <pegium/parser/RecoveryTrace.hpp>
#include <pegium/parser/TerminalRecoverySupport.hpp>
#include <pegium/parser/TextUtils.hpp>
#include <ranges>
#include <optional>
#include <string>

namespace pegium::parser {

template <auto literal, bool case_sensitive = true>
struct Literal final : grammar::Literal {
  static_assert(std::ranges::none_of(literal, [](char c) { return c == '\0'; }),
                "Literal cannot contain '\\0'.");
  static constexpr bool nullable = literal.empty();
  static constexpr bool isFailureSafe = true;
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

  using LocalRecoveryChoice = detail::TerminalRecoveryCandidate;
  using LocalRecoveryChoiceKind = detail::TerminalRecoveryChoiceKind;

  [[nodiscard]] static std::size_t
  wordContinuationLength(const char *cursor) noexcept {
    std::size_t length = 0u;
    while (isWord(*cursor)) {
      ++length;
      ++cursor;
    }
    return length;
  }

  [[nodiscard]] static constexpr std::uint32_t
  wordBoundarySplitPenalty(std::size_t continuationLength) noexcept {
    if (continuationLength <= 1u) {
      return 2u;
    }
    if (continuationLength == 2u) {
      return 1u;
    }
    return 0u;
  }

  [[nodiscard]] static const char *
  fuzzyInputEnd(const char *cursor, const char *limit) noexcept {
    if (cursor == nullptr || cursor >= limit) {
      return cursor;
    }

    if constexpr (recover_word_boundary_split) {
      const char *it = cursor;
      while (it < limit && isWord(*it)) {
        ++it;
      }
      return it;
    } else {
      constexpr std::size_t kExtraWindow = 4u;
      const auto span = static_cast<std::size_t>(limit - cursor);
      return cursor +
             std::min(span, static_cast<std::size_t>(literal.size() +
                                                     kExtraWindow));
    }
  }

  [[nodiscard]] static std::string_view
  fuzzyInputView(const char *cursor, const char *limit) noexcept {
    const auto *viewEnd = fuzzyInputEnd(cursor, limit);
    return {cursor, static_cast<std::size_t>(viewEnd - cursor)};
  }

  [[nodiscard]] static constexpr std::uint32_t
  maxAcceptableFuzzyCost() noexcept {
    return static_cast<std::uint32_t>(literal.size() + 3u);
  }

  [[nodiscard]] static constexpr bool
  shouldConsiderFuzzyCandidate(
      const detail::LiteralFuzzyCandidate &candidate) noexcept {
    return candidate.normalizedEditCost <= maxAcceptableFuzzyCost();
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
  [[nodiscard]] std::optional<detail::LiteralFuzzyCandidate>
  findFuzzyCandidate(Context &ctx, const char *cursorStart) const noexcept {
    if constexpr (!recover_fuzzy_literal) {
      (void)ctx;
      (void)cursorStart;
      return std::nullopt;
    } else {
      auto candidate = detail::find_best_literal_fuzzy_candidate(
          getValue(), fuzzyInputView(cursorStart, fuzzyInputLimit(ctx)),
          case_sensitive);
      if (candidate.has_value() && shouldConsiderFuzzyCandidate(*candidate)) {
        return candidate;
      }
      return std::nullopt;
    }
  }

  template <EditableParseModeContext Context,
            typename HasWordBoundaryViolation>
  [[nodiscard]] LocalRecoveryChoice
  selectLocalRecoveryChoice(
      Context &ctx, const char *cursorStart, const char *matchedEnd,
      const std::optional<detail::LiteralFuzzyCandidate> &fuzzyCandidate,
      HasWordBoundaryViolation &&hasWordBoundaryViolation) const {
    LocalRecoveryChoice bestChoice;
    auto considerChoice = [&bestChoice](const LocalRecoveryChoice &choice) {
      if (detail::is_better_terminal_recovery_candidate(choice, bestChoice)) {
        bestChoice = choice;
      }
    };

    if constexpr (recover_word_boundary_split) {
      if (matchedEnd != nullptr && hasWordBoundaryViolation(matchedEnd)) {
        considerChoice(detail::evaluate_insert_hidden_gap_terminal_candidate(
            ctx, cursorStart, matchedEnd, this,
            wordBoundarySplitPenalty(wordContinuationLength(matchedEnd))));
      }
    }

    if constexpr (recover_fuzzy_literal) {
      if (fuzzyCandidate.has_value()) {
        const char *const fuzzyEnd = cursorStart + fuzzyCandidate->consumed;
        if (detail::can_apply_recovery_match(ctx, fuzzyEnd) &&
            !hasWordBoundaryViolation(fuzzyEnd)) {
          considerChoice(detail::evaluate_replace_leaf_terminal_candidate(
              ctx, cursorStart, fuzzyEnd, this,
              detail::literal_fuzzy_replacement_cost(*fuzzyCandidate),
              fuzzyCandidate->distance, fuzzyCandidate->substitutionCount,
              fuzzyCandidate->distance));
        }
      }
    }

    if constexpr (recover_insertable) {
      const auto insertChoice =
          detail::evaluate_insert_hidden_terminal_candidate(ctx, cursorStart,
                                                            this);
      considerChoice(insertChoice);
      if (insertChoice.kind != LocalRecoveryChoiceKind::None) {
        return bestChoice;
      }
    }

    considerChoice(detail::evaluate_delete_scan_terminal_candidate(
        ctx, cursorStart,
        [this, &ctx, &hasWordBoundaryViolation](
            const char *scanStart) noexcept -> const char * {
          const char *const scanEnd = terminal(scanStart);
          if (scanEnd == nullptr ||
              !detail::can_apply_recovery_match(ctx, scanEnd) ||
              hasWordBoundaryViolation(scanEnd)) {
            return nullptr;
          }
          return scanEnd;
        },
        [this, &ctx](const char *scanEnd) { ctx.leaf(scanEnd, this); }));

    return bestChoice;
  }

  template <EditableParseModeContext Context,
            typename HasWordBoundaryViolation>
  bool applyLocalRecoveryChoice(
      Context &ctx, const LocalRecoveryChoice &choice,
      const std::optional<detail::LiteralFuzzyCandidate> &fuzzyCandidate,
      const char *cursorStart, const char *matchedEnd,
      HasWordBoundaryViolation &&hasWordBoundaryViolation) const {
    using enum LocalRecoveryChoiceKind;
    switch (choice.kind) {
    case WordBoundarySplit:
      if (matchedEnd != nullptr && hasWordBoundaryViolation(matchedEnd) &&
          detail::apply_insert_hidden_gap_recovery_edit(ctx, matchedEnd,
                                                        this)) {
        if constexpr (RecoveryParseModeContext<Context>) {
          PEGIUM_RECOVERY_TRACE("[literal rule] split word boundary for '",
                                getValue(), "' at ",
                                static_cast<TextOffset>(matchedEnd - ctx.begin));
        }
        return true;
      }
      return false;
    case Fuzzy: {
      if (!fuzzyCandidate.has_value()) {
        return false;
      }
      const char *const fuzzyEnd = cursorStart + fuzzyCandidate->consumed;
      if (detail::can_apply_recovery_match(ctx, fuzzyEnd) &&
          !hasWordBoundaryViolation(fuzzyEnd) &&
          detail::apply_replace_leaf_recovery_edit(
              ctx, fuzzyEnd, this,
              detail::literal_fuzzy_replacement_cost(*fuzzyCandidate))) {
        if constexpr (RecoveryParseModeContext<Context>) {
          PEGIUM_RECOVERY_TRACE("[literal rule] fuzzy match '", getValue(),
                                "' at ", ctx.cursorOffset(),
                                " distance=", fuzzyCandidate->distance,
                                " cost=", fuzzyCandidate->normalizedEditCost);
        }
        return true;
      }
      return false;
    }
    case InsertHidden:
      if constexpr (recover_insertable) {
        if (detail::apply_insert_hidden_recovery_edit(ctx, this)) {
          if constexpr (RecoveryParseModeContext<Context>) {
            PEGIUM_RECOVERY_TRACE("[literal rule] inserted '", getValue(),
                                  "' at ", ctx.cursorOffset());
          }
          return true;
        }
      }
      return false;
    case DeleteScan:
      return detail::recover_by_delete_scan(
          ctx,
          [this, &ctx, &hasWordBoundaryViolation](
              const char *scanStart) noexcept -> const char * {
            const char *const scanEnd = terminal(scanStart);
            if (scanEnd == nullptr ||
                !detail::can_apply_recovery_match(ctx, scanEnd) ||
                hasWordBoundaryViolation(scanEnd)) {
              return nullptr;
            }
            return scanEnd;
          },
          [this, &ctx](const char *scanEnd) {
            if constexpr (RecoveryParseModeContext<Context>) {
              PEGIUM_RECOVERY_TRACE("[literal rule] delete-scan match '",
                                    getValue(), "' at ", ctx.cursorOffset());
            }
            ctx.leaf(scanEnd, this);
          });
    case None:
      return false;
    }
    return false;
  }

  bool probe_impl(ParseContext &ctx) const noexcept {
    const char *const matchEnd = terminal(ctx.cursor());
    if (matchEnd == nullptr) {
      return false;
    }
    if constexpr (isWord(literal.back())) {
      if (isWord(*matchEnd)) {
        return false;
      }
    }
    return true;
  }

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    const char *const cursorStart = ctx.cursor();
    const char *const matchedEnd = terminal(cursorStart);
    const auto hasWordBoundaryViolation = [this](const char *end) noexcept {
      if constexpr (!literal.empty() && isWord(literal.back())) {
        return end != nullptr && isWord(*end);
      } else {
        (void)end;
        return false;
      }
    };

    if constexpr (StrictParseModeContext<Context>) {
      if (matchedEnd == nullptr || hasWordBoundaryViolation(matchedEnd)) {
        return false;
      }
      PEGIUM_RECOVERY_TRACE("[literal rule] direct match '", getValue(), "' at ",
                            ctx.cursorOffset());
      ctx.leaf(matchedEnd, this);
      return true;
    } else if constexpr (RecoveryParseModeContext<Context>) {
      if (matchedEnd != nullptr && !hasWordBoundaryViolation(matchedEnd)) {
        PEGIUM_RECOVERY_TRACE("[literal rule] direct match '", getValue(),
                              "' at ", ctx.cursorOffset());
        ctx.leaf(matchedEnd, this);
        return true;
      }
      if (!ctx.canEdit()) {
        return false;
      }
      const auto fuzzyCandidate = findFuzzyCandidate(ctx, cursorStart);
      const auto bestChoice = selectLocalRecoveryChoice(
          ctx, cursorStart, matchedEnd, fuzzyCandidate,
          hasWordBoundaryViolation);
      if (applyLocalRecoveryChoice(ctx, bestChoice, fuzzyCandidate, cursorStart,
                                   matchedEnd, hasWordBoundaryViolation)) {
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
      const auto anchorDistance =
          static_cast<std::size_t>(ctx.anchor - cursorStart);
      if (cursorStart < ctx.anchor && anchorDistance <= literal.size()) {
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

      const auto fuzzyCandidate = findFuzzyCandidate(ctx, cursorStart);
      const auto bestChoice = selectLocalRecoveryChoice(
          ctx, cursorStart, matchedEnd, fuzzyCandidate,
          hasWordBoundaryViolation);
      return applyLocalRecoveryChoice(ctx, bestChoice, fuzzyCandidate,
                                      cursorStart, matchedEnd,
                                      hasWordBoundaryViolation);
    }
  }

public:
  bool probeRecoverable(RecoveryContext &ctx) const noexcept {
    const char *const cursorStart = ctx.cursor();
    if constexpr (recover_word_boundary_split) {
      if (const char *const matchedEnd = terminal(cursorStart);
          matchedEnd != nullptr && isWord(*matchedEnd)) {
        return true;
      }
    }
    if constexpr (recover_fuzzy_literal) {
      const auto fuzzyCandidate = findFuzzyCandidate(ctx, cursorStart);
      return fuzzyCandidate.has_value();
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

  constexpr bool isNullable() const noexcept override {
    return nullable;
  }

private:
  // Keep generic recovery able to synthesize simple punctuation, but avoid
  // inventing multi-character tokens.
  static constexpr bool recover_insertable =
      literal.size() == 1 && !isWord(literal.back());
  static constexpr bool recover_word_boundary_split =
      literal.size() > 0 && isWord(literal.back());
  static constexpr bool recover_fuzzy_literal = literal.size() > 1;

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
