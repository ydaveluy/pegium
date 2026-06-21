#pragma once

/// Parser rule template implementing precedence-aware infix parsing.

#include <array>
#include <concepts>
#include <memory>
#include <optional>
#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/grammar/InfixRule.hpp>
#include <pegium/core/grammar/ParserRule.hpp>
#include <pegium/core/parser/ExpectContext.hpp>
#include <pegium/core/parser/ExpectFrontier.hpp>
#include <pegium/core/parser/InfixRuleSupport.hpp>
#include <pegium/core/parser/NodeParseHelpers.hpp>
#include <pegium/core/parser/ParseAttempt.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/ParseExpression.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/ParserRule.hpp>
#include <pegium/core/parser/RawValueTraits.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>
#include <pegium/core/parser/RecoveryUtils.hpp>
#include <pegium/core/parser/RecoveryTrace.hpp>
#include <pegium/core/parser/StepTrace.hpp>
#include <pegium/core/parser/AstReflectionBootstrap.hpp>
#include <pegium/core/parser/ValueBuildContext.hpp>
#include <string>
#include <type_traits>
#include <utility>

namespace pegium::parser::detail {

/// Closed list of `Infix` recovery candidate families. Private to
/// `InfixRule` — no other combinator references this vocabulary.
///
/// Strict-path discipline: recovery-side; none of these types are
/// constructed on the strict-only nominal path.
enum class InfixCandidateFamily : std::uint8_t {
  /// The tail continues: a strict operator at the cursor followed
  /// by RHS strict or recoverable acceptance.
  ContinueTail,
  /// The tail recovers from operator-position noise: a bounded
  /// delete of pre-operator junk, after which a strict RHS primary
  /// starts.
  DeleteOperatorNoise,
};

} // namespace pegium::parser::detail

namespace pegium::parser {
template <NonNullableExpression Element,
          grammar::InfixOperator::Associativity Assoc>
struct InfixOperator : grammar::InfixOperator {
private:
  static constexpr bool operator_has_required_raw_values =
      detail::expression_raw_compliant_v<Element>;

public:
  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe =
      !IsOrderedChoice<Element>::value &&
      std::remove_cvref_t<Element>::isFailureSafe;
  static constexpr Associativity kAssociativity = Assoc;

  static_assert(
      operator_has_required_raw_values,
      "InfixOperator requires getRawValue(node): direct element must provide "
      "a specific typed value (not grammar::RuleValue); for OrderedChoice, "
      "each choice must provide getRawValue(node) with a specific typed value.");

  explicit constexpr InfixOperator(Element &&element)
    requires(!std::is_lvalue_reference_v<Element>)
      : _element(std::move(element)) {}

  explicit constexpr InfixOperator(Element element)
    requires(std::is_lvalue_reference_v<Element>)
      : _element(element) {}

  constexpr InfixOperator(InfixOperator &&) noexcept = default;
  constexpr InfixOperator(const InfixOperator &) = default;
  constexpr InfixOperator &operator=(InfixOperator &&) noexcept = default;
  constexpr InfixOperator &operator=(const InfixOperator &) = default;

  Associativity getAssociativity() const noexcept override {
    return kAssociativity;
  }
  std::int32_t getPrecedence() const noexcept override { return _precedence; }
  const grammar::AbstractElement *getOperator() const noexcept override {
    return std::addressof(_element);
  }

  template <typename Visitor>
  bool visitRawValue(const CstNodeView &node, const ValueBuildContext &context,
                     Visitor &&visitor) const {
    return detail::InfixOperatorValueSupport<Element>::visit_raw_value(
        _element, node, context, std::forward<Visitor>(visitor));
  }

  constexpr void setPrecedence(std::int32_t p) noexcept { _precedence = p; }
  constexpr bool isNullable() const noexcept override {
    return nullable;
  }

  const char *terminal(const char *begin) const noexcept
    requires TerminalCapableExpression<Element>
  {
    return _element.terminal(begin);
  }

private:
  friend struct detail::ParseAccess;
  friend struct detail::FastProbeAccess;
  friend struct detail::InitAccess;

  void init_impl(AstReflectionInitContext &ctx) const { parser::init(_element, ctx); }

  // Strict-mode (ParseContext/TrackedParseContext) and recovery-mode operator
  // lookaheads share one fast-probe body. `has_operator_fast_probe` sits on the
  // strict infix hot path: when the wrapped operator expression already exposes
  // a specialized fast probe (for example literals or ordered choices of
  // literals), this avoids a checkpoint for every operator lookahead.
  template <typename Context>
    requires StrictParseModeContext<Context> || RecoveryParseModeContext<Context>
  bool fast_probe_impl(Context &ctx) const {
    return parser::attempt_fast_probe(ctx, _element);
  }

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    if constexpr (IsOrderedChoice<Element>::value) {
      return detail::parse_wrapped_node(ctx, this, _element);
    }
    return detail::parse_overriding_first_child(ctx, this, _element);
  }

  ExpressionHolder<Element> _element;
  std::int32_t _precedence = -1;
};

template <NonNullableExpression Element>
constexpr auto LeftAssociation(Element &&element) {
  return InfixOperator<Element, grammar::InfixOperator::Associativity::Left>{
      std::forward<Element>(element)};
}
template <NonNullableExpression Element>
constexpr auto RightAssociation(Element &&element) {
  return InfixOperator<Element, grammar::InfixOperator::Associativity::Right>{
      std::forward<Element>(element)};
}

// -----------------------------------------------------------------------------
// Concept: InfixOperatorExpression
// -----------------------------------------------------------------------------
template <class E>
concept InfixOperatorExpression =
    std::derived_from<std::remove_cvref_t<E>, grammar::InfixOperator>;

// -----------------------------------------------------------------------------
// InfixRule wrapper
// -----------------------------------------------------------------------------
template <typename T, auto Left, auto Op, auto Right>
  requires DefaultConstructibleAstNode<T>
struct InfixRule final : grammar::InfixRule {
  struct Ops {
    AstNode *(*getValue)(const void *, const CstNodeView &, AstNode *,
                         const ValueBuildContext &);
    bool (*rule)(const void *, ParseContext &);
    bool (*ruleTracked)(const void *, TrackedParseContext &);
    bool (*fastProbeTracked)(const void *, TrackedParseContext &);
    bool (*recover)(const void *, RecoveryContext &);
    bool (*expect)(const void *, ExpectContext &);
    bool (*probeRecoverable)(const void *, RecoveryContext &);
    void (*init)(const void *, AstReflectionInitContext &);
    const grammar::AbstractElement *(*elem)(const void *) noexcept;
    const grammar::InfixOperator *(*getOperator)(const void *,
                                                 std::size_t) noexcept;
    std::size_t (*operatorCount)(const void *) noexcept;
    void (*destroy)(void *) noexcept;
    void *(*clone)(const void *);
    // Re-point a Model's captured owner after the enclosing InfixRule is copied
    // or moved: the Model stores `_owner` (the grammar::InfixRule passed to
    // ctx.exit), which must follow the new InfixRule instance, not stay pinned
    // to the copied-from / moved-from one (which may be destroyed).
    void (*rebindOwner)(void *, const grammar::InfixRule *) noexcept;
  };

  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe = false;
  using type = T;


  // ------------------- ctor -------------------
  template <NonNullableExpression Element, InfixOperatorExpression... Operator>
    requires(IsParserRule<Element>)
  constexpr InfixRule(std::string_view name, Element &&element,
                      Operator &&...oper)
      : _name{name} {
    using W = Model<Element, Operator...>;

    reset();
    _ops = W::opsTable;

    _obj = new W(this, std::forward<Element>(element),
                 std::forward<Operator>(oper)...);
  }

  ~InfixRule() override { reset(); }

  // ------------------- API grammar::InfixRule -------------------
  std::string_view getTypeName() const noexcept override {
    static constexpr auto typeName = detail::type_name_v<T>;
    return typeName;
  }

  const grammar::InfixOperator *
  getOperator(std::size_t index) const noexcept override {
    assert(_obj && _ops.getOperator && "Missing element wrapper!");
    return _ops.getOperator(_obj, index);
  }

  std::size_t operatorCount() const noexcept override {
    assert(_obj && _ops.operatorCount && "Missing element wrapper!");
    return _ops.operatorCount(_obj);
  }
  constexpr bool isNullable() const noexcept override {
    return nullable;
  }

  const grammar::AbstractElement *getElement() const noexcept override {
    assert(_obj && _ops.elem && "Missing element wrapper!");
    return _ops.elem(_obj);
  }
  std::string_view getName() const noexcept override { return _name; }
  InfixRule(const InfixRule &other)
      : _name(other._name), _ops(other._ops) {
    if (other._obj) {
      _obj = _ops.clone(other._obj);
      _ops.rebindOwner(_obj, this);
    }
  }

  InfixRule(InfixRule &&other) noexcept { move_from(std::move(other)); }

  InfixRule &operator=(const InfixRule &other) {
    if (this == &other)
      return *this;

    reset();

    _name = other._name;
    _ops = other._ops;

    if (other._obj) {
      _obj = _ops.clone(other._obj);
      _ops.rebindOwner(_obj, this);
    }
    return *this;
  }

  InfixRule &operator=(InfixRule &&other) noexcept {
    if (this == &other)
      return *this;
    reset();
    move_from(std::move(other));
    return *this;
  }

  AstNode *getValue(const CstNodeView &node, AstNode *lhsNode,
                    const ValueBuildContext &context) const override {
    assert(_obj && _ops.getValue && "Missing element wrapper!");
    return _ops.getValue(_obj, node, lhsNode, context);
  }

  bool probeRecoverable(RecoveryContext &ctx) const {
    assert(_obj && _ops.probeRecoverable && "Missing recovery probe wrapper!");
    return _ops.probeRecoverable(_obj, ctx);
  }

private:
  friend struct detail::ParseAccess;
  friend struct detail::FastProbeAccess;
  friend struct detail::InitAccess;

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    if constexpr (std::same_as<std::remove_cvref_t<Context>, ParseContext>) {
      assert(_obj && _ops.rule && "Missing element wrapper!");
      return _ops.rule(_obj, ctx);
    } else if constexpr (std::same_as<std::remove_cvref_t<Context>,
                                      TrackedParseContext>) {
      assert(_obj && _ops.ruleTracked && "Missing tracked infix wrapper!");
      return _ops.ruleTracked(_obj, ctx);
    } else if constexpr (RecoveryParseModeContext<Context>) {
      assert(_obj && _ops.recover && "Missing element wrapper!");
      return _ops.recover(_obj, ctx);
    } else {
      assert(_obj && _ops.expect && "Missing element wrapper!");
      return _ops.expect(_obj, ctx);
    }
  }

  bool fast_probe_impl(TrackedParseContext &ctx) const {
    assert(_obj && _ops.fastProbeTracked && "Missing tracked infix fast-probe wrapper!");
    return _ops.fastProbeTracked(_obj, ctx);
  }

  void init_impl(AstReflectionInitContext &ctx) const {
    ctx.registerProducedType(detail::ast_node_type_info<T>());
    assert(_obj && _ops.init && "Missing infix init wrapper!");
    _ops.init(_obj, ctx);
  }

  void reset() noexcept {
    if (!_obj) {
      return;
    }
    _ops.destroy(_obj);
    _obj = nullptr;
    _ops = {};
  }

  void move_from(InfixRule &&other) noexcept {
    _name = std::exchange(other._name, {});
    _obj = std::exchange(other._obj, nullptr);
    _ops = std::exchange(other._ops, {});
    if (_obj) {
      _ops.rebindOwner(_obj, this);
    }
  };

  template <NonNullableExpression Element, InfixOperatorExpression... Operators>
  struct Model {
    using Assoc = grammar::InfixOperator::Associativity;
    using Traits =
        detail::InfixRuleModelTraits<T, Left, Op, Right, Element, Operators...>;
    static constexpr std::int32_t kMaxPrecedence = Traits::kMaxPrecedence;
    using OpType = typename Traits::OpType;
    using LeftPointeeType = typename Traits::LeftPointeeType;
    using RightPointeeType = typename Traits::RightPointeeType;

  private:
    struct ParseEngine;
    using ValueBuilder = detail::InfixRuleValueBuilder<
        Model, T, Left, Op, Right, OpType, LeftPointeeType, RightPointeeType,
        Operators...>;
    using OperatorCatalog = detail::InfixRuleOperatorCatalog<Model, Operators...>;
    using ExpectSupport = detail::InfixRuleExpectSupport<Model, Operators...>;

  public:
    explicit Model(const grammar::InfixRule *owner, Element &&elem,
                   Operators &&...opsIn)
        : _owner(owner), primary(detail::move_if_owned<Element>(elem)),
          ops(OperatorCatalog::make_ops(detail::move_if_owned<Operators>(opsIn)...)) {}

    static AstNode *getValue(const void *self, const CstNodeView &node,
                             AstNode *lhsNode,
                             const ValueBuildContext &context) {
      return ValueBuilder::getValue(static_cast<const Model *>(self), node,
                                    lhsNode, context);
    }

    static bool rule(const void *self, ParseContext &ctx) {
      return ParseEngine::rule(static_cast<const Model *>(self), ctx);
    }
    static bool ruleTracked(const void *self, TrackedParseContext &ctx) {
      return ParseEngine::rule(static_cast<const Model *>(self), ctx);
    }
    static bool fastProbeTracked(const void *self, TrackedParseContext &ctx) {
      const auto *model = static_cast<const Model *>(self);
      return parser::attempt_fast_probe(ctx, model->primary);
    }
    static bool recover(const void *self, RecoveryContext &ctx) {
      return ParseEngine::recover(static_cast<const Model *>(self), ctx);
    }
    static bool expect(const void *self, ExpectContext &ctx) {
      return ParseEngine::expect(static_cast<const Model *>(self), ctx);
    }
    static bool probeRecoverable(const void *self, RecoveryContext &ctx) {
      const auto *model = static_cast<const Model *>(self);
      detail::ProbeRestoreScope guard{ctx};
      const auto startOffset = ctx.cursorOffset();
      ctx.restoreMaxCursor(ctx.cursor());
      const bool matchedPrimary =
          parser::attempt_parse_no_edits(ctx, model->primary);
      return matchedPrimary || ctx.maxCursorOffset() > startOffset;
    }
    static void init(const void *self, AstReflectionInitContext &ctx) {
      const auto *model = static_cast<const Model *>(self);
      parser::init(model->primary, ctx);
      std::apply(
          [&ctx](const auto &...operatorExpression) {
            (parser::init(operatorExpression, ctx), ...);
          },
          model->ops);
    }

    static const grammar::AbstractElement *elem(const void *self) noexcept {
      return std::addressof(static_cast<const Model *>(self)->primary);
    }

    static const grammar::InfixOperator *
    getOperator(const void *self, std::size_t index) noexcept {
      return OperatorCatalog::getOperator(static_cast<const Model *>(self), index);
    }

    static std::size_t operatorCount(const void *) noexcept {
      return OperatorCatalog::operatorCount();
    }

    static void destroy(void *self) noexcept {
      delete static_cast<Model *>(self);
    }
    static void *clone(const void *self) {
      return new Model(*static_cast<const Model *>(self));
    }
    static void rebindOwner(void *self,
                            const grammar::InfixRule *owner) noexcept {
      static_cast<Model *>(self)->_owner = owner;
    }

    using Ops = InfixRule::Ops;

    static constexpr Ops make_ops() noexcept {
      return {
          .getValue = &Model::getValue,
          .rule = &Model::rule,
          .ruleTracked = &Model::ruleTracked,
          .fastProbeTracked = &Model::fastProbeTracked,
          .recover = &Model::recover,
          .expect = &Model::expect,
          .probeRecoverable = &Model::probeRecoverable,
          .init = &Model::init,
          .elem = &Model::elem,
          .getOperator = &Model::getOperator,
          .operatorCount = &Model::operatorCount,
          .destroy = &Model::destroy,
          .clone = &Model::clone,
          .rebindOwner = &Model::rebindOwner,
      };
    }

    inline static constexpr Ops opsTable = make_ops();

  private:
    struct ParseEngine {
      struct EditableTailCandidate {
        detail::InfixCandidateFamily kind =
            detail::InfixCandidateFamily::DeleteOperatorNoise;
        std::uint32_t rhsPrefixNoiseDeleteCount = 0;
        bool rhsPrimaryObservedWithoutEdits = false;
        bool legal = false;
      };

      struct EditableTailReplayResult {
        bool matched = true;
        // Only `recover_stray_operator_run` ever sets this true (to halt the
        // tail loop once a stray-operator-run repair has consumed the rest of
        // the editable expression); every other replay path leaves it false or
        // forwards a nested result unchanged.
        bool stopTail = false;
      };

      struct EditableTailObservation {
        TextOffset rhsStartOffset = 0;
        std::uint32_t rhsPrefixNoiseDeleteCount = 0;
        // Populated before the observation's `ctx.rewind()` so the
        // caller can still project the speculative match onto a
        // `RecoveryKey` after the speculative edits have been rolled
        // back.
        std::uint32_t immediateEditCount = 0;
        std::uint32_t immediateEditCost = 0;
        TextOffset firstImmediateEditOffset =
            std::numeric_limits<TextOffset>::max();
        TextOffset postMatchCursorOffset = 0;
        // Real codepoint-delete count captured by the observable
        // `observe_stray_operator_run_delete_count` scan from the
        // pre-operator-match cursor. Used to project a real
        // `RecoveryKey` for the `DeleteOperatorNoise` candidate.
        // Empty when the scan would not admit a recovery from this
        // cursor.
        std::optional<std::uint32_t> strayOperatorRunDeleteCount;
        TextOffset strayOperatorRunFirstEditOffset = 0;
        bool exposedPrimary = true;
        bool matchedPrimary = false;
        bool advancedPrimary = false;
        bool rhsPrimaryObservedWithoutEdits = false;
        bool multipleImmediateEditsAtStart = false;
      };

      template <StrictParseModeContext Context>
      static bool rule(const Model *model, Context &ctx) {
        const auto expressionStart = ctx.cursor();
        if (!parse(model->primary, ctx)) {
          return false;
        }
        ctx.skip();
        return parse_tail_strict(model, ctx, /*minPrecedence=*/0,
                                 expressionStart);
      }

      static bool recover(const Model *model, RecoveryContext &ctx) {
        const auto expressionStart = ctx.cursor();
        if (!parse(model->primary, ctx)) {
          return false;
        }
        ctx.skip();
        const auto replayResult =
            parse_tail_editable(model, ctx, /*minPrecedence=*/0, expressionStart);
        return replayResult.matched;
      }

      static bool expect(const Model *model, ExpectContext &ctx) {
        const auto expressionStart = ctx.cursor();
        if (!parse(model->primary, ctx)) {
          return false;
        }
        if (ctx.frontierBlocked()) {
          return true;
        }
        ctx.skip();
        return parse_tail_expect(model, ctx, /*minPrecedence=*/0,
                                 expressionStart);
      }

    private:
      template <StrictParseModeContext Context>
      static bool parse_tail_strict(const Model *model, Context &ctx,
                                    std::int32_t minPrecedence,
                                    const char *lhsStart) {
        if (minPrecedence > kMaxPrecedence) {
          return true;
        }
        while (true) {
          if (!has_operator_fast_probe(model, ctx, minPrecedence)) {
            return true;
          }
          const auto nodeCheckpoint = ctx.mark();
          (void)ctx.enter();
          std::int32_t nextMinPrecedence = 0;
          if (!try_match_operator_strict(model, ctx, minPrecedence,
                                         nextMinPrecedence)) {
            ctx.rewind(nodeCheckpoint);
            return true;
          }
          ctx.skip();
          const auto rhsStart = ctx.cursor();
          if (!parse(model->primary, ctx)) {
            ctx.rewind(nodeCheckpoint);
            return true;
          }
          ctx.skip();
          (void)parse_tail_strict(model, ctx, nextMinPrecedence, rhsStart);
          assert(model->_owner && "The owner must be set");
          ctx.exit(lhsStart, model->_owner);
        }
      }

      static EditableTailReplayResult
      parse_tail_editable(const Model *model, RecoveryContext &ctx,
                          std::int32_t minPrecedence, const char *lhsStart) {
        if (minPrecedence > kMaxPrecedence) {
          return {};
        }
        while (true) {
          if (!has_operator_fast_probe(model, ctx, minPrecedence)) {
            return {};
          }
          const auto nodeCheckpoint = ctx.mark();
          (void)ctx.enter();
          std::int32_t nextMinPrecedence = 0;
          if (!try_match_operator_editable(model, ctx, minPrecedence,
                                           nextMinPrecedence)) {
            ctx.rewind(nodeCheckpoint);
            return {};
          }
          ctx.skip();
          const auto rhsStart = ctx.cursor();
          const char *const directRhsSavedFurthest =
              ctx.maxCursor();
          if (parser::attempt_parse_no_edits(ctx, model->primary) &&
              ctx.cursor() != rhsStart) {
            ctx.skip();
            const auto nestedReplayResult =
                parse_tail_editable(model, ctx, nextMinPrecedence, rhsStart);
            assert(model->_owner && "The owner must be set");
            ctx.exit(lhsStart, model->_owner);
            if (!nestedReplayResult.matched || nestedReplayResult.stopTail) {
              return nestedReplayResult;
            }
            continue;
          }
          ctx.rewind(nodeCheckpoint);
          ctx.restoreMaxCursor(directRhsSavedFurthest);
          // Observable stray-operator scan from the pre-operator-match cursor
          // (the same cursor `recover_stray_operator_run` reads when
          // committed). It is deliberately lazy: when the RHS primary parses
          // strictly, no tail-recovery candidate is enumerated, so the scan
          // would only add recovery-mode cost to a nominal operator chain.
          const auto strayOperatorRunCursorOffset = ctx.cursorOffset();
          const auto strayOperatorRunDeleteCount =
              observe_stray_operator_run_delete_count(model, ctx,
                                                       minPrecedence);
          (void)ctx.enter();
          nextMinPrecedence = 0;
          if (!try_match_operator_editable(model, ctx, minPrecedence,
                                           nextMinPrecedence)) {
            ctx.rewind(nodeCheckpoint);
            return {};
          }
          auto observation = observe_editable_rhs_primary(model, ctx);
          observation.strayOperatorRunDeleteCount =
              strayOperatorRunDeleteCount;
          observation.strayOperatorRunFirstEditOffset =
              strayOperatorRunCursorOffset;
          const auto candidates =
              enumerate_editable_tail_candidates(observation);
          const auto *selected =
              select_editable_tail_candidate_by_recovery_key(candidates,
                                                             observation);
          if (selected == nullptr) {
            ctx.rewind(nodeCheckpoint);
            return {.matched = false, .stopTail = false};
          }
          if (selected->kind ==
              detail::InfixCandidateFamily::DeleteOperatorNoise) {
            ctx.rewind(nodeCheckpoint);
            return recover_stray_operator_run(model, ctx, minPrecedence);
          }
          const auto replayResult = replay_editable_rhs_primary(
              model, ctx, *selected, nextMinPrecedence, lhsStart);
          if (!replayResult.matched || replayResult.stopTail) {
            return replayResult;
          }
        }
      }

      static bool parse_tail_expect(const Model *model, ExpectContext &ctx,
                                    std::int32_t minPrecedence,
                                    const char *lhsStart) {
        if (minPrecedence > kMaxPrecedence) {
          return true;
        }
        while (true) {
          if (ExpectSupport::try_merge_operator_frontier(model, ctx,
                                                         minPrecedence)) {
            ctx.clearFrontierBlock();
            return true;
          }

          const auto nodeCheckpoint = ctx.mark();
          std::int32_t nextMinPrecedence = 0;
          if (!ExpectSupport::try_match_operator(model, ctx, minPrecedence,
                                                 nextMinPrecedence)) {
            ctx.rewind(nodeCheckpoint);
            return true;
          }
          ctx.skip();
          const auto rhsStart = ctx.cursor();
          auto rhsNoEditGuard = ctx.withEditTrackingDisabled();
          (void)rhsNoEditGuard;
          if (!parse(model->primary, ctx)) {
            ctx.rewind(nodeCheckpoint);
            return true;
          }
          if (ctx.frontierBlocked()) {
            return true;
          }
          ctx.skip();
          if (!parse_tail_expect(model, ctx, nextMinPrecedence, rhsStart)) {
            return false;
          }
          if (ctx.frontierBlocked()) {
            return true;
          }
          assert(model->_owner && "The owner must be set");
          ctx.exit(lhsStart, model->_owner);
        }
      }

      template <std::size_t I = 0, ParseModeContext Context>
      static bool has_operator_fast_probe(const Model *model, Context &ctx,
                                          std::int32_t minPrecedence) {
        const auto &op = std::get<I>(model->ops);
        constexpr auto precedence =
            static_cast<std::int32_t>(sizeof...(Operators) - I);
        if (precedence >= minPrecedence &&
            parser::attempt_fast_probe(ctx, op)) {
          return true;
        }
        if constexpr (I + 1 == sizeof...(Operators)) {
          return false;
        } else {
          return has_operator_fast_probe<I + 1>(model, ctx, minPrecedence);
        }
      }

      template <std::size_t I = 0, StrictParseModeContext Context>
      static bool try_match_operator_strict(const Model *model, Context &ctx,
                                            std::int32_t minPrecedence,
                                            std::int32_t &nextMinPrecedence) {
        const auto &op = std::get<I>(model->ops);
        constexpr auto precedence =
            static_cast<std::int32_t>(sizeof...(Operators) - I);
        if (precedence < minPrecedence) {
          return false;
        }
        if (parser::attempt_fast_probe(ctx, op) &&
            parser::attempt_parse_strict(ctx, op)) {
          nextMinPrecedence =
              detail::infix_next_min_precedence<decltype(op)>(precedence);
          return true;
        }
        if constexpr (I + 1 == sizeof...(Operators)) {
          return false;
        } else {
          return try_match_operator_strict<I + 1>(model, ctx, minPrecedence,
                                                  nextMinPrecedence);
        }
      }

      template <std::size_t I = 0>
      static bool try_match_operator_editable(const Model *model,
                                              RecoveryContext &ctx,
                                              std::int32_t minPrecedence,
                                              std::int32_t &nextMinPrecedence) {
        const auto &op = std::get<I>(model->ops);
        constexpr auto precedence =
            static_cast<std::int32_t>(sizeof...(Operators) - I);
        if (precedence < minPrecedence) {
          return false;
        }
        if (!parser::attempt_fast_probe(ctx, op)) {
          if constexpr (I + 1 == sizeof...(Operators)) {
            return false;
          } else {
            return try_match_operator_editable<I + 1>(model, ctx, minPrecedence,
                                                      nextMinPrecedence);
          }
        }
        // RAII restore of the edit permissions: an exception inside
        // `attempt_parse_editable` (e.g. cancellation) must not leave the
        // context with both flags pinned to false.
        auto editGuard = ctx.withEditPermissions(false, false);
        (void)editGuard;
        if (const bool matched = parser::attempt_parse_editable(ctx, op);
            matched) {
          nextMinPrecedence =
              detail::infix_next_min_precedence<decltype(op)>(precedence);
          return true;
        }
        if constexpr (I + 1 == sizeof...(Operators)) {
          return false;
        } else {
          return try_match_operator_editable<I + 1>(model, ctx, minPrecedence,
                                                    nextMinPrecedence);
        }
      }

      [[nodiscard]] static EditableTailObservation
      observe_editable_rhs_primary(const Model *model,
                                   RecoveryContext &ctx) {
        EditableTailObservation observation;
        detail::ProbeRestoreScope guard{ctx};
        ctx.restoreMaxCursor(ctx.cursor());
        ctx.skip();
        // Enter the prefix-noise cleanup whenever the RHS primary does not
        // strictly start here — stray operators OR arbitrary garbage — so the
        // infix tail resyncs to the operand the same way recovery deletes noise
        // everywhere else.
        if (!parser::attempt_fast_probe(ctx, model->primary) &&
            !observe_rhs_prefix_operator_noise(model, ctx, observation)) {
          observation.exposedPrimary = false;
          return observation;
        }
        const char *const rhsStart = ctx.cursor();
        observation.rhsStartOffset = ctx.cursorOffset();
        const std::size_t baseRecoveryEditCount = ctx.recoveryEditCount();
        {
          detail::ProbeRestoreScope noEditGuard{ctx};
          if (parser::attempt_parse_no_edits(ctx, model->primary) &&
              ctx.cursor() != rhsStart) {
            observation.matchedPrimary = true;
            observation.advancedPrimary = true;
            observation.rhsPrimaryObservedWithoutEdits = true;
            observation.postMatchCursorOffset = ctx.cursorOffset();
            return observation;
          }
        }
        observation.matchedPrimary = parse(model->primary, ctx);
        observation.advancedPrimary = ctx.cursor() != rhsStart;
        observation.postMatchCursorOffset = ctx.cursorOffset();
        if (!observation.matchedPrimary || !observation.advancedPrimary) {
          return observation;
        }
        if (ctx.recoveryEditCount() == baseRecoveryEditCount) {
          return observation;
        }
        const auto edits = ctx.recoveryEditsView();
        observation.immediateEditCount = static_cast<std::uint32_t>(
            ctx.recoveryEditCount() - baseRecoveryEditCount);
        // Capture the first-edit offset and the aggregate edit cost so
        // the caller can project the ContinueTail candidate onto a
        // `RecoveryKey` after the rewind has rolled back the edits.
        observation.firstImmediateEditOffset =
            edits[baseRecoveryEditCount].offset;
        for (std::size_t i = baseRecoveryEditCount;
             i < ctx.recoveryEditCount(); ++i) {
          observation.immediateEditCost +=
              detail::default_edit_cost(edits[i].kind);
        }
        // The InfixRule tail should match `many(op + Primary)` semantics: the
        // rhs primary is matched *starting at rhsStart*. Reject the ContinueTail
        // candidate when the speculative match instead looks like a grammar
        // reinterpretation, in either of two ways:
        //  - multiple edits all anchored at rhsStart, or
        //  - any edit committed *before* rhsStart, which back-edited an earlier
        //    token of the input to reshape it (e.g. inserting `(` at offset 23
        //    to reinterpret `c-4+;` as `c(-4)+;`).
        // Such multi-edit speculative cascades routinely beat the outer
        // recovery, so they must not win the tail here.
        observation.multipleImmediateEditsAtStart =
            (observation.immediateEditCount > 1u &&
             observation.firstImmediateEditOffset ==
                 observation.rhsStartOffset) ||
            observation.firstImmediateEditOffset <
                observation.rhsStartOffset;
        return observation;
      }


      [[nodiscard]] static bool
      continue_editable_tail_candidate_is_legal(
          const EditableTailObservation &observation) noexcept {
        return observation.exposedPrimary && observation.matchedPrimary &&
               observation.advancedPrimary &&
               !observation.multipleImmediateEditsAtStart;
      }

      [[nodiscard]] static std::array<EditableTailCandidate, 2>
      enumerate_editable_tail_candidates(
          const EditableTailObservation &observation) noexcept {
        // ContinueTail is admissible exactly when a strict operator at the
        // cursor is followed by a strict-or-recoverable RHS primary; that
        // legality is computed by
        // `continue_editable_tail_candidate_is_legal(observation)`.
        const bool continueTailLegal =
            continue_editable_tail_candidate_is_legal(observation);
        // DeleteOperatorNoise is admissible exactly when the bounded
        // operator-noise scan committed a real delete count (which already
        // implies both a following strict RHS primary and an available
        // delete budget at the pre-operator cursor).
        const bool deleteNoiseLegal =
            observation.strayOperatorRunDeleteCount.has_value();

        return {{
            {.kind = detail::InfixCandidateFamily::ContinueTail,
             .rhsPrefixNoiseDeleteCount = observation.rhsPrefixNoiseDeleteCount,
             .rhsPrimaryObservedWithoutEdits =
                 observation.rhsPrimaryObservedWithoutEdits,
             .legal = continueTailLegal},
            {.kind = detail::InfixCandidateFamily::DeleteOperatorNoise,
             .legal = deleteNoiseLegal},
        }};
      }

      /// Project each legal candidate onto the shared `RecoveryKey` and
      /// select the best — the same comparator used across the rest of
      /// recovery. Returns a pointer to the winning candidate, or nullptr
      /// when none is legal.
      ///
      /// ContinueTail's key comes from the observation (already computed,
      /// costs captured before the speculative rewind).
      ///
      /// DeleteOperatorNoise's key comes from the bounded, rewound
      /// observation. The winner path still replays the scan before
      /// committing edits, so observation and replay stay separate.
      [[nodiscard]] static const EditableTailCandidate *
      select_editable_tail_candidate_by_recovery_key(
          const std::array<EditableTailCandidate, 2> &candidates,
          const EditableTailObservation &observation) {
        const EditableTailCandidate *selected = nullptr;
        detail::RecoveryKey bestKey;
        bool anySelected = false;

        const auto considerCandidate = [&](const EditableTailCandidate &cand,
                                           const detail::RecoveryKey &key) {
          if (!anySelected ||
              detail::is_better_recovery_key(key, bestKey)) {
            anySelected = true;
            bestKey = key;
            selected = std::addressof(cand);
          }
        };

        for (const auto &candidate : candidates) {
          if (!candidate.legal) {
            continue;
          }
          if (candidate.kind == detail::InfixCandidateFamily::ContinueTail) {
            const auto effectiveFirstEdit = detail::effective_first_edit_offset(
                observation.immediateEditCount != 0u,
                observation.firstImmediateEditOffset,
                observation.postMatchCursorOffset);
            const detail::RecoveryKey key{
                .matched = observation.matchedPrimary,
                .firstEditOffset = effectiveFirstEdit,
                .editCost = observation.immediateEditCost,
                .editCount = observation.immediateEditCount,
                .progressAfterEdits = observation.postMatchCursorOffset,
            };
            considerCandidate(candidate, key);
          } else {
            // Real DeleteOperatorNoise cost from the observable scan
            // (`observe_stray_operator_run_delete_count`). The
            // candidate's `RecoveryKey` reflects the actual codepoint
            // count the committed `recover_stray_operator_run` would
            // produce. The candidate's `legal` flag (already filtered
            // above) is `strayOperatorRunDeleteCount.has_value()`, so the
            // optional is guaranteed engaged here.
            constexpr std::uint32_t kSingleDeleteCost =
                detail::default_edit_cost(ParseDiagnosticKind::Deleted);
            const auto deleteCount =
                *observation.strayOperatorRunDeleteCount;
            const auto realEditCost = deleteCount * kSingleDeleteCost;
            const detail::RecoveryKey key{
                .matched = true,
                // The scan's first edit lands at the operator position
                // (cursor at the start of the run, i.e.
                // `strayOperatorRunFirstEditOffset` captured before
                // `try_match_operator_editable` advanced the cursor).
                .firstEditOffset =
                    observation.strayOperatorRunFirstEditOffset,
                .editCost = realEditCost,
                .editCount = deleteCount,
                .progressAfterEdits = observation.rhsStartOffset,
            };
            considerCandidate(candidate, key);
          }
        }
        return selected;
      }

      static EditableTailReplayResult
      replay_editable_rhs_primary(const Model *model, RecoveryContext &ctx,
                                  const EditableTailCandidate &candidate,
                                  std::int32_t nextMinPrecedence,
                                  const char *lhsStart) {
        assert(candidate.kind == detail::InfixCandidateFamily::ContinueTail &&
               "Only a ContinueTail candidate can replay an infix RHS");
        ctx.skip();
        const bool replayedPrefixNoise = replay_rhs_prefix_operator_noise(
            model, ctx, candidate.rhsPrefixNoiseDeleteCount);
        assert(replayedPrefixNoise &&
               "Infix RHS prefix replay must match observed cleanup");
        if (!replayedPrefixNoise) {
          return {.matched = false, .stopTail = false};
        }
        const auto rhsStart = ctx.cursor();
        bool matchedPrimary = false;
        if (candidate.rhsPrimaryObservedWithoutEdits) {
          matchedPrimary = parser::attempt_parse_no_edits(ctx, model->primary);
        } else {
          matchedPrimary = parse(model->primary, ctx);
        }
        if (!matchedPrimary || ctx.cursor() == rhsStart) {
          // Speculative observation can disagree with replay when a
          // sibling recovery path mutates state (max cursor, failure
          // history, fingerprint-affecting edits) between the observation
          // and the replay. Reject the candidate and let the enclosing
          // recovery attempt compete normally instead of asserting.
          return {.matched = false, .stopTail = false};
        }
        ctx.skip();
        const auto nestedReplayResult =
            parse_tail_editable(model, ctx, nextMinPrecedence, rhsStart);
        assert(model->_owner && "The owner must be set");
        ctx.exit(lhsStart, model->_owner);
        return {.matched = true, .stopTail = nestedReplayResult.stopTail};
      }

      /// Observable variant of `recover_stray_operator_run`: runs the
      /// same delete-scan logic against the cursor without committing
      /// any mutation (cursor, skipAfterDelete, edits all rewound), and
      /// returns the number of codepoint deletes that the scan would
      /// produce on commit. `std::nullopt` means the scan would not
      /// admit a `DeleteOperatorNoise` recovery from the current cursor
      /// (operator probe fails or the scan runs past its budget without
      /// matching). Used to project a real `RecoveryKey` for the
      /// `DeleteOperatorNoise` candidate at selection time.
      /// Shared bounded delete-scan over a stray operator run: skip operator
      /// noise until the operator probe re-accepts, unless a recoverable
      /// primary hides behind a local trivia gap. The caller owns the
      /// skip-after-delete policy (and, for observation, the rewind guard) and
      /// supplies the on-match sink (`recover_by_guarded_delete_scan`
      /// dispatches the 1- vs 2-argument sink).
      template <typename OnMatch>
      [[nodiscard]] static bool
      scan_stray_operator_run(const Model *model, RecoveryContext &ctx,
                              std::int32_t minPrecedence, OnMatch &&onMatch) {
        const auto hidesRecoverablePrimaryBehindLocalGap = [&]() {
          detail::ProbeRestoreScope guard{ctx};
          const char *const gapStart = ctx.cursor();
          const char *const gapEnd = ctx.skip_without_builder(gapStart);
          ctx.skip();
          // A "real" separator spans more than a single trivia *codepoint*.
          // Counting bytes (`gapEnd > gapStart + 1`) wrongly treats one
          // multi-byte whitespace codepoint (U+00A0, U+3000, …) as a gap; count
          // codepoints so the test is independent of UTF-8 width. Bounded to two
          // decodes — we only need "> 1 codepoint" — to stay cheap on this path.
          std::size_t gapCodepoints = 0u;
          for (const char *p = gapStart; p < gapEnd && gapCodepoints < 2u;) {
            const auto length = pegium::utils::utf8_codepoint_length(*p);
            p += length == 0u ? 1u : length;
            ++gapCodepoints;
          }
          return gapCodepoints > 1u && ctx.cursor() < ctx.end &&
                 (parser::attempt_fast_probe(ctx, model->primary) ||
                  parser::probe_locally_recoverable(model->primary, ctx));
        };
        return detail::recover_by_guarded_delete_scan(
            ctx,
            [&]() {
              return has_operator_fast_probe(model, ctx, minPrecedence) ||
                     !hidesRecoverablePrimaryBehindLocalGap();
            },
            [&](const char *scanCursor) -> const char * {
              if (scanCursor == ctx.end ||
                  has_operator_fast_probe(model, ctx, minPrecedence) ||
                  hidesRecoverablePrimaryBehindLocalGap()) {
                return nullptr;
              }
              return scanCursor;
            },
            std::forward<OnMatch>(onMatch));
      }

      [[nodiscard]] static std::optional<std::uint32_t>
      observe_stray_operator_run_delete_count(const Model *model,
                                              RecoveryContext &ctx,
                                              std::int32_t minPrecedence) {
        if (!has_operator_fast_probe(model, ctx, minPrecedence)) {
          return std::nullopt;
        }
        std::optional<std::uint32_t> capturedDeleteCount;
        {
          detail::ProbeRestoreScope guard{ctx};
          // Disable `skipAfterDelete` for the duration of the stray-operator-run
          // probe. Without this flip, the bounded delete scan would absorb a
          // hidden separator that crosses a logical boundary and commit to a
          // wrong run length. The mutation is observed by
          // `RecoveryPolicyFingerprint` so the recovery cache cannot collapse a
          // hit across the flipped state.
          detail::ScopedBoolOverride skipPolicy{ctx.skipAfterDelete, false};
          (void)scan_stray_operator_run(
              model, ctx, minPrecedence,
              [&](const char *, std::uint32_t deleteCount) {
                capturedDeleteCount = deleteCount;
              });
        }
        return capturedDeleteCount;
      }

      static EditableTailReplayResult
      recover_stray_operator_run(const Model *model, RecoveryContext &ctx,
                                 std::int32_t minPrecedence) {
        detail::ScopedBoolOverride skipPolicy{ctx.skipAfterDelete, false};
        bool stopTail = false;
        bool recovered = false;
        // Short-circuit is load-bearing: the side-effectful stray-operator scan
        // must run only when the cheap fast probe succeeds. Expressed as an
        // explicit `if` (rather than `probe && scan(...)`) so the side effect is
        // not buried in the right-hand operand of `&&`.
        if (has_operator_fast_probe(model, ctx, minPrecedence)) {
          recovered = scan_stray_operator_run(
              model, ctx, minPrecedence, [&](const char *matchedCursor) {
                stopTail =
                    ctx.skip_without_builder(matchedCursor) > matchedCursor;
              });
        }
        return {.matched = recovered, .stopTail = recovered && stopTail};
      }

      static bool observe_rhs_prefix_operator_noise(
          const Model *model, RecoveryContext &ctx,
          EditableTailObservation &observation) {
        const auto deleteCount =
            observe_rhs_prefix_operator_noise_delete_count(model, ctx);
        if (!deleteCount.has_value()) {
          return false;
        }
        observation.rhsPrefixNoiseDeleteCount = *deleteCount;
        const bool replayedPrefixNoise = replay_rhs_prefix_operator_noise(
            model, ctx, observation.rhsPrefixNoiseDeleteCount);
        assert(replayedPrefixNoise &&
               "Observed infix RHS prefix cleanup must replay during observation");
        return replayedPrefixNoise;
      }

      [[nodiscard]] static std::optional<std::uint32_t>
      observe_rhs_prefix_operator_noise_delete_count(const Model *model,
                                                     RecoveryContext &ctx) {
        const auto checkpoint = ctx.mark();
        std::optional<std::uint32_t> observedDeleteCount;
        {
          // LocalRhsNoiseCleanup contract: skipAfterDelete=false (so the
          // bounded scan does not absorb a hidden separator) AND
          // allowDelete=true (so the probe works even when an outer
          // InsertRetry parent disabled deletes — InfixRule's RHS noise
          // delete is a grammar-driven affordance of the Pratt pattern,
          // bounded to one short of the global delete budget, not free
          // delete permission). Both mutations are visible in
          // `RecoveryPolicyFingerprint`.
          detail::ScopedBoolOverride skipGuard{ctx.skipAfterDelete, false};
          auto editGuard = ctx.withEditPermissions(ctx.allowInsert, true);
          // The scan runs to the FULL delete budget so an operator-noise run
          // whose primary abuts it exactly at the budget is observed. The
          // conservative cap (one short of the budget) is re-applied below, but
          // ONLY to a primary reached across skipper trivia — that primary may
          // belong to the next construct, so it stays capped (the don't-fold-the-
          // next-statement invariant). A primary that abuts the noise with no
          // trivia is the glued operator-typo case and is absorbed up to the
          // full budget. Derive the cap from the budget so it auto-scales with
          // whatever budget the grammar/options configure.
          const auto maxProbeDeletes =
              ctx.maxConsecutiveCodepointDeletes > 1u
                  ? ctx.maxConsecutiveCodepointDeletes - 1u
                  : 1u;
          bool primaryAbutsNoise = false;
          // Set once the scan has crossed skipper trivia and landed on a
          // non-primary token: the deleted run is then no longer contiguous
          // with what follows, so we stop rather than keep deleting across the
          // separator (which would swallow a structural token like `;` and fold
          // the next construct into this RHS — a statement merge).
          bool crossedSeparatorWithoutPrimary = false;
          (void)detail::recover_by_guarded_delete_scan(
              ctx,
              [&]() {
                // Delete a codepoint when it is a stray operator OR contiguous
                // non-primary garbage. Operators are safe to delete across
                // trivia (a typo operator run is often space-separated and is
                // never a structural anchor); arbitrary garbage is only deleted
                // while still contiguous (no separator crossed), so the scan
                // cannot walk past a `;` into the next construct. The strict-
                // primary matchFn below stops the scan the instant the operand
                // becomes reachable.
                if (ctx.cursor() == ctx.end) {
                  return false;
                }
                if (has_operator_fast_probe(model, ctx, /*minPrecedence=*/0)) {
                  return true;
                }
                return !crossedSeparatorWithoutPrimary &&
                       !parser::attempt_fast_probe(ctx, model->primary);
              },
              [&](const char *) -> const char * {
                const char *const beforeSkip = ctx.cursor();
                ctx.skip();
                // The primary abuts the noise only when no skipper trivia was
                // crossed to reach it (the cursor did not advance on skip).
                const bool abutsHere = ctx.cursor() == beforeSkip;
                // Strict probe only: the purpose of the scan is to find a
                // position where `primary` strictly starts. Allowing
                // `probe_locally_recoverable` here would stop after a
                // single delete on any run of operator-like noise (the
                // recoverable probe is too permissive at operator
                // positions), leaving the remaining stray operators
                // unhandled by ContinueTail.
                if (parser::attempt_fast_probe(ctx, model->primary)) {
                  primaryAbutsNoise = abutsHere;
                  return ctx.cursor();
                }
                // Landed on a non-primary token across trivia. If it is not a
                // stray operator either, it is a structural anchor (a `;`, a
                // closing token), so stop — the scan must not delete past it
                // into the next construct. A stray operator across trivia stays
                // deletable via the canDeleteStep above.
                if (!abutsHere &&
                    !has_operator_fast_probe(model, ctx, /*minPrecedence=*/0)) {
                  crossedSeparatorWithoutPrimary = true;
                }
                return nullptr;
              },
              [&](const char *, std::uint32_t deleteCount) {
                observedDeleteCount = deleteCount;
              },
              {.maxDeletes = ctx.maxConsecutiveCodepointDeletes,
               .allowOverflow = false});
          // Keep an over-cap observation only when the primary abuts the noise:
          // a glued operator-typo run cannot fold a following construct into
          // this RHS, whereas a primary reachable only across trivia (a space or
          // a newline) might, so it stays bounded by the conservative cap.
          if (observedDeleteCount.has_value() &&
              *observedDeleteCount > maxProbeDeletes && !primaryAbutsNoise) {
            observedDeleteCount.reset();
          }
        }
        ctx.rewind(checkpoint);
        return observedDeleteCount;
      }

      static bool replay_rhs_prefix_operator_noise(const Model *model,
                                                   RecoveryContext &ctx,
                                                   std::uint32_t deleteCount) {
        if (deleteCount == 0u) {
          return true;
        }
        // Same policy as observe_rhs_prefix_operator_noise_delete_count:
        // the commit replay must see the same skipAfterDelete=false /
        // allowDelete=true scope so it commits exactly what the probe
        // sanctioned.
        detail::ScopedBoolOverride skipGuard{ctx.skipAfterDelete, false};
        auto editGuard = ctx.withEditPermissions(ctx.allowInsert, true);
        return detail::recover_by_guarded_delete_scan(
            ctx,
            [&]() {
              // Mirror observe_rhs_prefix_operator_noise_delete_count's relaxed
              // predicate so observe/replay delete the exact same run.
              return ctx.cursor() != ctx.end &&
                     !parser::attempt_fast_probe(ctx, model->primary);
            },
            [&](const char *, std::uint32_t deletedCount) {
              ctx.skip();
              return deletedCount == deleteCount ? ctx.cursor() : nullptr;
            },
            [](const char *) { /* no per-step progress action */ },
            {.maxDeletes = deleteCount, .allowOverflow = false});
      }

    };

  public:
    const grammar::InfixRule *_owner = nullptr;
    ExpressionHolder<Element> primary;
    std::tuple<Operators...> ops;
  };

  // ------------------- data -------------------
  std::string _name{};

  void *_obj = nullptr;
  Ops _ops{};
};

} // namespace pegium::parser
