#pragma once

#include <concepts>
#include <limits>
#include <pegium/core/grammar/Repetition.hpp>
#include <pegium/core/parser/CompletionSupport.hpp>
#include <pegium/core/parser/EditableRecoverySupport.hpp>
#include <pegium/core/parser/ExpectContext.hpp>
#include <pegium/core/parser/ExpectFrontier.hpp>
#include <pegium/core/parser/ParseAttempt.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/ParseExpression.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>
#include <pegium/core/parser/RecoveryTrace.hpp>
#include <pegium/core/parser/SkipperBuilder.hpp>
#include <pegium/core/parser/StepTrace.hpp>
#include <string>

namespace pegium::parser {

template <std::size_t min, std::size_t max, NonNullableExpression Element>
struct RepetitionWithSkipper;

template <std::size_t min, std::size_t max, NonNullableExpression Element>
struct Repetition : grammar::Repetition {
  static constexpr bool nullable = min == 0;
  static constexpr bool isFailureSafe =
      min == 0 ||
      (min == 1 &&
       (max != 1 || std::remove_cvref_t<Element>::isFailureSafe));
  static_assert(!(min == 0 && max == 0),
                "A Repetition cannot have both min and max set to 0.");

  explicit constexpr Repetition(Element &&element)
    requires(!std::is_lvalue_reference_v<Element>)
      : _element(std::move(element)) {}

  explicit constexpr Repetition(Element element)
    requires(std::is_lvalue_reference_v<Element>)
      : _element(element) {}

  constexpr Repetition(Repetition &&) noexcept = default;
  constexpr Repetition(const Repetition &) = default;
  constexpr Repetition &operator=(Repetition &&) noexcept = default;
  constexpr Repetition &operator=(const Repetition &) = default;

  const AbstractElement *getElement() const noexcept override {
    return std::addressof(_element);
  }
  std::size_t getMin() const noexcept override { return min; }
  std::size_t getMax() const noexcept override { return max; }

private:
  friend struct detail::ParseAccess;
  friend struct detail::InitAccess;

  static constexpr bool is_optional = (min == 0 && max == 1);
  static constexpr bool is_star =
      (min == 0 && max == std::numeric_limits<std::size_t>::max());
  static constexpr bool is_plus =
      (min == 1 && max == std::numeric_limits<std::size_t>::max());
  static constexpr bool is_fixed = (min == max && min > 0);

  struct ExpectIterationResult {
    bool matched = false;
    bool blocked = false;
    bool progressed = false;
  };

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    if constexpr (StrictParseModeContext<Context>) {
      if constexpr (is_optional) {
        (void)attempt_parse_strict(ctx, _element);
        return true;
      } else if constexpr (is_star) {
        if (!attempt_parse_strict(ctx, _element)) {
          return true;
        }
        auto checkpointAfterItem = ctx.mark();
        while (true) {
          ctx.skip();
          if (!attempt_parse_strict(ctx, _element)) {
            ctx.rewind(checkpointAfterItem);
            return true;
          }
          checkpointAfterItem = ctx.mark();
        }
      } else if constexpr (is_plus) {
        if (!attempt_parse_strict(ctx, _element)) {
          return false;
        }
        auto checkpointAfterItem = ctx.mark();
        while (true) {
          ctx.skip();
          if (!attempt_parse_strict(ctx, _element)) {
            ctx.rewind(checkpointAfterItem);
            return true;
          }
          checkpointAfterItem = ctx.mark();
        }
      } else if constexpr (is_fixed) {
        if (!parse(_element, ctx)) {
          return false;
        }
        for (std::size_t repetitionIndex = 1; repetitionIndex < min;
             ++repetitionIndex) {
          ctx.skip();
          if (!parse(_element, ctx)) {
            return false;
          }
        }
        return true;
      } else if constexpr (min == 0) {
        if (!attempt_parse_strict(ctx, _element)) {
          return true;
        }
        auto checkpointAfterItem = ctx.mark();
        std::size_t repetitionCount = 1;
        for (; repetitionCount < max; ++repetitionCount) {
          ctx.skip();
          if (!attempt_parse_strict(ctx, _element)) {
            ctx.rewind(checkpointAfterItem);
            return true;
          }
          checkpointAfterItem = ctx.mark();
        }
        return true;
      } else {
        if (!attempt_parse_strict(ctx, _element)) {
          return false;
        }
        auto checkpointAfterItem = ctx.mark();
        std::size_t repetitionCount = 1;
        for (; repetitionCount < min; ++repetitionCount) {
          ctx.skip();
          if (!attempt_parse_strict(ctx, _element)) {
            ctx.rewind(checkpointAfterItem);
            return false;
          }
          checkpointAfterItem = ctx.mark();
        }

        for (; repetitionCount < max; ++repetitionCount) {
          ctx.skip();
          if (!attempt_parse_strict(ctx, _element)) {
            ctx.rewind(checkpointAfterItem);
            return true;
          }
          checkpointAfterItem = ctx.mark();
        }
        return true;
      }
    } else if constexpr (RecoveryParseModeContext<Context>) {
      detail::stepTraceInc(detail::StepCounter::RepetitionRecoverCalls);
      auto try_insertable_recovery_iteration =
          [this, &ctx](bool skipBetweenIterations) {
        return try_recovery_iteration(ctx, skipBetweenIterations);
      };
      if constexpr (is_optional) {
        const auto checkpoint = ctx.mark();
        const char *const savedMaxCursor = ctx.maxCursor();
        if (attempt_parse_no_edits(ctx, _element)) {
          if (savedMaxCursor > ctx.maxCursor()) {
            ctx.restoreMaxCursor(savedMaxCursor);
          }
          return true;
        }
        ctx.rewind(checkpoint);
        ctx.restoreMaxCursor(savedMaxCursor);
        const bool prefixLooksStarted =
            attempt_fast_probe(ctx, _element) ||
            probe_locally_recoverable(_element, ctx);
        // Keep optional branches conservative in recovery: only try to edit the
        // optional element when its prefix is already locally plausible. This
        // avoids inventing an optional construct that was never started.
        if (!prefixLooksStarted) {
          return true;
        }
        if (try_recovery_iteration(ctx, /*skipBetweenIterations=*/false)) {
          return true;
        }
        ctx.rewind(checkpoint);
        return false;
      } else if constexpr (is_star) {
        PEGIUM_RECOVERY_TRACE("[repeat * rule] enter offset=",
                              ctx.cursorOffset());
        bool matchedAny = false;
        while (try_insertable_recovery_iteration(matchedAny)) {
          matchedAny = true;
          PEGIUM_RECOVERY_TRACE("[repeat * rule] element matched offset=",
                                ctx.cursorOffset());
        }
        PEGIUM_RECOVERY_TRACE("[repeat * rule] stop offset=", ctx.cursorOffset());
        return true;
      } else if constexpr (is_plus) {
        if (!try_insertable_recovery_iteration(/*skipBetweenIterations=*/false)) {
          PEGIUM_RECOVERY_TRACE("[repeat + rule] first element failed offset=",
                                ctx.cursorOffset());
          return false;
        }
        PEGIUM_RECOVERY_TRACE("[repeat + rule] first element ok offset=",
                              ctx.cursorOffset());
        while (try_insertable_recovery_iteration(/*skipBetweenIterations=*/true)) {
          PEGIUM_RECOVERY_TRACE("[repeat + rule] element matched offset=",
                                ctx.cursorOffset());
        }
        PEGIUM_RECOVERY_TRACE("[repeat + rule] stop offset=", ctx.cursorOffset());
        return true;
      } else if constexpr (is_fixed) {
        for (std::size_t repetitionIndex = 0; repetitionIndex < min;
             ++repetitionIndex) {
          if (repetitionIndex > 0) {
            ctx.skip();
          }
          if (!parse(_element, ctx)) {
            return false;
          }
        }
        return true;
      } else {
        for (std::size_t repetitionIndex = 0; repetitionIndex < min;
             ++repetitionIndex) {
          if (repetitionIndex > 0) {
            ctx.skip();
          }
          if (!parse(_element, ctx)) {
            return false;
          }
        }
        for (std::size_t repetitionCount = min; repetitionCount < max;
             ++repetitionCount) {
          if (!try_insertable_recovery_iteration(repetitionCount > 0)) {
            break;
          }
        }
        return true;
      }
    } else {
      if constexpr (is_optional) {
        if (const auto result = try_expect_iteration(ctx, /*skipBefore=*/false);
            result.matched && result.blocked) {
          ctx.clearFrontierBlock();
        }
        return true;
      } else if constexpr (is_star) {
        while (true) {
          const auto result = try_expect_iteration(ctx, /*skipBefore=*/false);
          if (!result.matched) {
            return true;
          }
          if (result.blocked) {
            ctx.clearFrontierBlock();
            return true;
          }
          if (!result.progressed) {
            return true;
          }
          ctx.skip();
        }
      } else if constexpr (is_plus) {
        const auto first = try_expect_iteration(ctx, /*skipBefore=*/false);
        if (!first.matched) {
          return false;
        }
        if (first.blocked) {
          return true;
        }
        while (true) {
          const auto result = try_expect_iteration(ctx, /*skipBefore=*/true);
          if (!result.matched) {
            return true;
          }
          if (result.blocked) {
            ctx.clearFrontierBlock();
            return true;
          }
          if (!result.progressed) {
            return true;
          }
        }
      } else {
        std::size_t repetitionCount = 0;
        for (; repetitionCount < min; ++repetitionCount) {
          if (repetitionCount > 0) {
            ctx.skip();
          }
          if (!parse(_element, ctx)) {
            return false;
          }
          if (ctx.frontierBlocked()) {
            return true;
          }
        }
        for (; repetitionCount < max; ++repetitionCount) {
          const auto result = try_expect_iteration(ctx, /*skipBefore=*/true);
          if (!result.matched) {
            return true;
          }
          if (result.blocked) {
            ctx.clearFrontierBlock();
            return true;
          }
          if (!result.progressed) {
            return true;
          }
        }
        return true;
      }
    }
  }

public:
  void init_impl(AstReflectionInitContext &ctx) const { parser::init(_element, ctx); }

private:

  bool try_recovery_iteration(RecoveryContext &ctx,
                              bool skipBetweenIterations) const {
    enum class RecoveryIterationChoice : std::uint8_t {
      None,
      NoInsert,
      DeleteRetry,
      Insert,
    };
    enum class RecoveryRetryPolicy : std::uint8_t {
      None,
      InsertOnly,
      DeleteAndInsert,
    };
    struct IterationRecoveryCandidates {
      detail::ProgressRecoveryCandidate noInsert;
      detail::ProgressRecoveryCandidate deleteRetry;
      detail::ProgressRecoveryCandidate insert;

      [[nodiscard]] RecoveryIterationChoice select() const noexcept {
        if (insert.matched) {
          auto choice = RecoveryIterationChoice::Insert;
          if (noInsert.matched &&
              detail::is_better_progress_recovery_candidate(noInsert, insert)) {
            choice = RecoveryIterationChoice::NoInsert;
            if (deleteRetry.matched &&
                detail::is_better_progress_recovery_candidate(deleteRetry,
                                                              noInsert)) {
              choice = RecoveryIterationChoice::DeleteRetry;
            }
          }
          if (choice == RecoveryIterationChoice::Insert &&
              deleteRetry.matched &&
              detail::is_better_progress_recovery_candidate(deleteRetry,
                                                            insert)) {
            choice = RecoveryIterationChoice::DeleteRetry;
          }
          return choice;
        }
        if (deleteRetry.matched) {
          return RecoveryIterationChoice::DeleteRetry;
        }
        if (noInsert.matched) {
          return RecoveryIterationChoice::NoInsert;
        }
        return RecoveryIterationChoice::None;
      }
    };
    struct NoInsertProbe {
      bool matched = false;
      bool progressed = false;
      bool usedEdits = false;
      bool reachedEndOfInput = false;
    };
    struct ProbeRecoveryPolicy {
      bool keepNoInsertCandidate = false;
      bool acceptNoInsertImmediately = false;
      RecoveryRetryPolicy retryPolicy = RecoveryRetryPolicy::None;

      [[nodiscard]] constexpr bool triesDeleteRetry() const noexcept {
        return retryPolicy == RecoveryRetryPolicy::DeleteAndInsert;
      }

      [[nodiscard]] constexpr bool triesInsertRetry() const noexcept {
        return retryPolicy != RecoveryRetryPolicy::None;
      }

      [[nodiscard]] constexpr bool
      disablesDeleteDuringInsertRetry(const NoInsertProbe &probe) const noexcept {
        return !probe.progressed &&
               retryPolicy == RecoveryRetryPolicy::InsertOnly;
      }

      [[nodiscard]] constexpr bool
      stopsNaturally(const NoInsertProbe &probe) const noexcept {
        return !probe.progressed && retryPolicy == RecoveryRetryPolicy::None;
      }

      [[nodiscard]] constexpr bool acceptsInsertCandidate(
          const NoInsertProbe &probe,
          std::uint32_t editCountDelta) const noexcept {
        if (!probe.reachedEndOfInput || triesDeleteRetry()) {
          return true;
        }
        return editCountDelta == 1u;
      }
    };

    const auto iterationCheckpoint = ctx.mark();
    const auto editCountBefore = ctx.currentEditCount();
    if (skipBetweenIterations) {
      ctx.skip();
    }
    const auto parseCheckpoint = ctx.mark();
    const char *const parseStart = ctx.cursor();
    const TextOffset parseStartOffset = ctx.cursorOffset();
    const char *const maxCursorBeforeAttempt = ctx.maxCursor();
    const TextOffset maxCursorBeforeAttemptOffset =
        static_cast<TextOffset>(maxCursorBeforeAttempt - ctx.begin);
    const TextOffset endOffset = static_cast<TextOffset>(ctx.end - ctx.begin);
    const TextOffset pendingRecoveryWindowBeginOffset =
        ctx.pendingRecoveryWindowBeginOffset();
    const TextOffset pendingRecoveryWindowMaxCursorOffset =
        ctx.pendingRecoveryWindowMaxCursorOffset();
    const bool previousAllowInsert = ctx.allowInsert;
    const bool previousAllowDelete = ctx.allowDelete;
    const auto restore_iteration_edit_permissions = [&ctx, previousAllowInsert,
                                                     previousAllowDelete]() {
      ctx.allowInsert = previousAllowInsert;
      ctx.allowDelete = previousAllowDelete;
    };
    const auto parse_without_new_insertions = [this, &ctx, parseStart]() {
      const bool allowInsertBefore = ctx.allowInsert;
      ctx.allowInsert = false;
      const bool matched = parse(_element, ctx) && ctx.cursor() != parseStart;
      ctx.allowInsert = allowInsertBefore;
      return matched;
    };

    const bool noInsertMatched = parse_without_new_insertions();
    const auto noInsertProbeProgress = detail::capture_recovery_probe_progress(ctx);
    const NoInsertProbe noInsertProbe = {
        .matched = noInsertMatched,
        .progressed = noInsertProbeProgress.committedProgressed(parseStartOffset),
        .usedEdits = ctx.currentEditCount() != editCountBefore,
        // `maxCursor` is intentionally not rewound. A probe can fail after
        // exploring deeper text and then rewind its committed cursor to the
        // iteration start.
        .reachedEndOfInput =
            noInsertProbeProgress.reachedEndOfInput(endOffset) &&
            noInsertProbeProgress.exploredBeyond(parseStartOffset) &&
            noInsertProbeProgress.exploredBeyond(maxCursorBeforeAttemptOffset),
    };
    const bool failedInsideActiveRecoveryWindow =
        !noInsertProbe.progressed && ctx.hasPendingRecoveryWindows() &&
        parseStartOffset >= pendingRecoveryWindowBeginOffset &&
        parseStartOffset < pendingRecoveryWindowMaxCursorOffset;
    const ProbeRecoveryPolicy probePolicy = [&]() constexpr noexcept {
      if (noInsertProbe.matched && noInsertProbe.progressed) {
        return ProbeRecoveryPolicy{
            .keepNoInsertCandidate = true,
            .acceptNoInsertImmediately =
                !noInsertProbe.usedEdits || !previousAllowInsert,
            .retryPolicy = previousAllowInsert ? RecoveryRetryPolicy::InsertOnly
                                               : RecoveryRetryPolicy::None,
        };
      }
      if (failedInsideActiveRecoveryWindow) {
        return ProbeRecoveryPolicy{
            .retryPolicy = RecoveryRetryPolicy::DeleteAndInsert,
        };
      }
      if (noInsertProbe.reachedEndOfInput) {
        return ProbeRecoveryPolicy{
            .retryPolicy = RecoveryRetryPolicy::InsertOnly,
        };
      }
      return ProbeRecoveryPolicy{};
    }();
    IterationRecoveryCandidates candidates;
    candidates.noInsert.matched = probePolicy.keepNoInsertCandidate;
    restore_iteration_edit_permissions();

    if (probePolicy.keepNoInsertCandidate) {
      candidates.noInsert =
          detail::capture_progress_recovery_candidate(ctx, iterationCheckpoint);
      if (probePolicy.acceptNoInsertImmediately) {
        return true;
      }
    }

    ctx.rewind(parseCheckpoint);
    restore_iteration_edit_permissions();
    if (probePolicy.stopsNaturally(noInsertProbe)) {
      ctx.rewind(iterationCheckpoint);
      return false;
    }

    const auto parse_delete_retry = [this, &ctx, parseStart]() {
      const bool allowInsertBefore = ctx.allowInsert;
      const bool allowDeleteBefore = ctx.allowDelete;
      ctx.allowInsert = false;
      const bool matched = parse(_element, ctx) && ctx.cursor() != parseStart;
      ctx.allowInsert = allowInsertBefore;
      ctx.allowDelete = allowDeleteBefore;
      return matched;
    };
    if (probePolicy.triesDeleteRetry()) {
      const bool matchedWithDeleteRetry = parse_delete_retry();
      if (matchedWithDeleteRetry) {
        candidates.deleteRetry =
            detail::capture_progress_recovery_candidate(ctx, iterationCheckpoint);
      }
      ctx.rewind(parseCheckpoint);
      restore_iteration_edit_permissions();
    }

    const auto parse_with_insert = [this, &ctx, parseStart,
                                    noInsertProbe,
                                    probePolicy]() {
      const bool allowDeleteBefore = ctx.allowDelete;
      if (probePolicy.disablesDeleteDuringInsertRetry(noInsertProbe)) {
        ctx.allowDelete = false;
      }
      const bool matched = parse(_element, ctx) && ctx.cursor() != parseStart;
      ctx.allowDelete = allowDeleteBefore;
      return matched;
    };
    bool matchedWithInsert = parse_with_insert();
    if (matchedWithInsert &&
        !probePolicy.acceptsInsertCandidate(
            noInsertProbe, ctx.currentEditCount() - editCountBefore)) {
      matchedWithInsert = false;
    }
    restore_iteration_edit_permissions();

    if (matchedWithInsert) {
      candidates.insert =
          detail::capture_progress_recovery_candidate(ctx, iterationCheckpoint);
    }
    const auto choice = candidates.select();

    if (choice == RecoveryIterationChoice::None) {
      ctx.rewind(iterationCheckpoint);
      return false;
    }

    // Rebuild only the winning branch from the shared parse checkpoint instead
    // of keeping success checkpoints alive across competing recovery parses.
    ctx.rewind(parseCheckpoint);
    restore_iteration_edit_permissions();
    switch (choice) {
    case RecoveryIterationChoice::NoInsert:
      return parse_without_new_insertions();
    case RecoveryIterationChoice::DeleteRetry:
      return parse_delete_retry();
    case RecoveryIterationChoice::Insert:
      return parse_with_insert();
    case RecoveryIterationChoice::None:
      break;
    }
    ctx.rewind(iterationCheckpoint);
    return false;
  }

public:

  constexpr const char *terminal(const char *begin) const noexcept
    requires TerminalCapableExpression<Element>
  {
    // optional
    if constexpr (is_optional) {
      const char *matchEnd = _element.terminal(begin);
      return matchEnd != nullptr ? matchEnd : begin;
    }
    // zero or more
    else if constexpr (is_star) {
      const char *cursor = begin;
      while (true) {
        const char *matchEnd = _element.terminal(cursor);
        if (matchEnd == nullptr) {
          break;
        }
        cursor = matchEnd;
      }
      return cursor;
    }
    // one or more
    else if constexpr (is_plus) {

      const char *cursor = _element.terminal(begin);
      if (cursor == nullptr)
        return cursor;

      while (true) {
        const char *matchEnd = _element.terminal(cursor);
        if (matchEnd == nullptr) {
          break;
        }
        cursor = matchEnd;
      }
      return cursor;
    }
    // only min/max times
    else if constexpr (is_fixed) {
      const char *cursor = begin;
      for (std::size_t repetitionIndex = 0; repetitionIndex < min;
           ++repetitionIndex) {
        cursor = _element.terminal(cursor);
        if (cursor == nullptr) {
          return cursor;
        }
      }
      return cursor;
    }
    // other cases
    else {

      const char *cursor = begin;
      std::size_t repetitionCount = 0;
      for (; repetitionCount < min; ++repetitionCount) {
        cursor = _element.terminal(cursor);
        if (cursor == nullptr) {
          return cursor;
        }
      }

      for (; repetitionCount < max; ++repetitionCount) {
        const char *matchEnd = _element.terminal(cursor);
        if (matchEnd == nullptr) {
          break;
        }
        cursor = matchEnd;
      }

      return cursor;
    }
  }
  constexpr const char *terminal(const std::string &text) const noexcept
    requires TerminalCapableExpression<Element>
  {
    return terminal(text.c_str());
  }

  constexpr bool isNullable() const noexcept override {
    return nullable;
  }

  template <std::convertible_to<Skipper> LocalSkipper>
    requires std::copy_constructible<ExpressionHolder<Element>>
  auto skip(LocalSkipper &&localSkipper) const & {
    return with_skipper(std::forward<LocalSkipper>(localSkipper));
  }

  template <std::convertible_to<Skipper> LocalSkipper>
  auto skip(LocalSkipper &&localSkipper) && {
    return std::move(*this).with_skipper(std::forward<LocalSkipper>(localSkipper));
  }

  template <typename... SkipperParts>
    requires((... && (detail::IsHiddenRules_v<SkipperParts> ||
                     detail::IsIgnoredRules_v<SkipperParts>))) &&
            std::copy_constructible<ExpressionHolder<Element>>
  auto skip(SkipperParts &&...parts) const & {
    return with_skipper(parser::skip(std::forward<SkipperParts>(parts)...));
  }

  template <typename... SkipperParts>
    requires((... && (detail::IsHiddenRules_v<SkipperParts> ||
                     detail::IsIgnoredRules_v<SkipperParts>)))
  auto skip(SkipperParts &&...parts) && {
    return std::move(*this).with_skipper(
        parser::skip(std::forward<SkipperParts>(parts)...));
  }

  template <std::convertible_to<Skipper> LocalSkipper>
    requires std::copy_constructible<ExpressionHolder<Element>>
  auto with_skipper(LocalSkipper &&localSkipper) const & {
    return RepetitionWithSkipper<min, max, Element>{
        *this, static_cast<Skipper>(std::forward<LocalSkipper>(localSkipper))};
  }

  template <std::convertible_to<Skipper> LocalSkipper>
  auto with_skipper(LocalSkipper &&localSkipper) && {
    return RepetitionWithSkipper<min, max, Element>{
        std::move(*this),
        static_cast<Skipper>(std::forward<LocalSkipper>(localSkipper))};
  }

private:
  ExpressionHolder<Element> _element;

  ExpectIterationResult try_expect_iteration(ExpectContext &ctx,
                                             bool skipBefore) const {
    const auto checkpoint = ctx.mark();
    if (skipBefore) {
      ctx.skip();
    }
    const auto parseCheckpoint = ctx.mark();
    if (!parse(_element, ctx)) {
      ctx.rewind(checkpoint);
      return {};
    }
    const auto frontier = capture_frontier_since(ctx, parseCheckpoint);
    return {
        .matched = true,
        .blocked = frontier.blocked,
        .progressed = ctx.cursor() != parseCheckpoint.cursor,
    };
  }

};

template <std::size_t min, std::size_t max, NonNullableExpression Element>
struct RepetitionWithSkipper final : Repetition<min, max, Element>,
                                     CompletionSkipperProvider {
  using Base = Repetition<min, max, Element>;
  static constexpr bool nullable = Base::nullable;
  static constexpr bool isFailureSafe = Base::isFailureSafe;

  explicit RepetitionWithSkipper(const Base &base, Skipper localSkipper)
      : Base(base), _localSkipper(std::move(localSkipper)) {}
  explicit RepetitionWithSkipper(Base &&base, Skipper localSkipper)
      : Base(std::move(base)), _localSkipper(std::move(localSkipper)) {}

  RepetitionWithSkipper(RepetitionWithSkipper &&) noexcept = default;
  RepetitionWithSkipper(const RepetitionWithSkipper &) = default;
  RepetitionWithSkipper &operator=(RepetitionWithSkipper &&) noexcept = default;
  RepetitionWithSkipper &operator=(const RepetitionWithSkipper &) = default;
  [[nodiscard]] const Skipper *
  getCompletionSkipper() const noexcept override {
    return std::addressof(_localSkipper);
  }

private:
  friend struct detail::ParseAccess;
  friend struct detail::InitAccess;

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    auto localSkipperGuard = ctx.with_skipper(_localSkipper);
    (void)localSkipperGuard;
    return parse(static_cast<const Base &>(*this), ctx);
  }

  void init_impl(AstReflectionInitContext &ctx) const {
    static_cast<const Base &>(*this).init_impl(ctx);
  }

  Skipper _localSkipper;
};

/// Make the `element` optional (repeated zero or one)
/// @tparam Element the expression to repeat
/// @param element the element to be optional
/// @return a repetition of zero or one `element`.
template <NonNullableExpression Element>
constexpr auto option(Element &&element) {
  return Repetition<0, 1, Element>{std::forward<Element>(element)};
}

/// Repeat the `element` zero or more
/// @tparam Element the expression to repeat
/// @param element the element to be repeated
/// @return a repetition of zero or more `element`.
template <NonNullableExpression Element>
constexpr auto many(Element &&element) {
  return Repetition<0, std::numeric_limits<std::size_t>::max(), Element>{
      std::forward<Element>(element)};
}

/// Repeat the `element` one or more
/// @tparam Element the expression to repeat
/// @param element the element to be repeated
/// @return a repetition of one or more `element`.
template <NonNullableExpression Element>
constexpr auto some(Element &&element) {
  return Repetition<1, std::numeric_limits<std::size_t>::max(), Element>{
      std::forward<Element>(element)};
}

/// Repeat the `element` one or more using a `separator`:
/// `element (separator element)*`
/// @tparam Element the expression to repeat
/// @tparam Sep the expression to use as separator
/// @param element the element to be repeated
/// @param separator the separator to be used between elements
/// @return a repetition of one or more `element` with a `separator`.
template <NonNullableExpression Element, Expression Sep>
constexpr auto some(Element &&element, Sep &&separator) {
  return std::forward<Element>(element) +
         many(std::forward<Sep>(separator) + std::forward<Element>(element));
}

/// Repeat the `element` zero or more using a `separator`
/// `(element (separator element)*)?`
/// @tparam Element the expression to repeat
/// @tparam Sep the expression to use as separator
/// @param element the element to be repeated
/// @param separator the separator to be used between elements
/// @return a repetition of zero or more `element` with a `separator`.
template <NonNullableExpression Element, Expression Sep>
constexpr auto many(Element &&element, Sep &&separator) {
  return option(
      some(std::forward<Element>(element), std::forward<Sep>(separator)));
}

/// Repeat the `element` `count` times.
/// @tparam count the count of repetitions
/// @tparam Element the expression to repeat
/// @param element the elements to be repeated
/// @return a repetition of `count` `element`.
template <std::size_t count, NonNullableExpression Element>
constexpr auto repeat(Element &&element) {
  return Repetition<count, count, Element>{std::forward<Element>(element)};
}

/// Repeat the `element` between `min` and `max` times.
/// @tparam min the min number of occurence (inclusive)
/// @tparam max the max number of occurence (inclusive)
/// @tparam Element the expression to repeat
/// @param element the elements to be repeated
/// @return a repetition of `min` to `max` `element`.
template <std::size_t min, std::size_t max, NonNullableExpression Element>
constexpr auto repeat(Element &&element) {
  return Repetition<min, max, Element>{std::forward<Element>(element)};
}

} // namespace pegium::parser
