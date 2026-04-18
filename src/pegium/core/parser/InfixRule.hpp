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
#include <pegium/core/parser/RecoveryUtils.hpp>
#include <pegium/core/parser/RecoveryTrace.hpp>
#include <pegium/core/parser/StepTrace.hpp>
#include <pegium/core/parser/AstReflectionBootstrap.hpp>
#include <pegium/core/parser/ValueBuildContext.hpp>
#include <string>
#include <type_traits>
#include <utility>

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
  // using Associativity = grammar::InfixOperator::Associativity;
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
  grammar::RuleValue getValue(const CstNodeView &node) const override {
    return detail::InfixOperatorValueSupport<Element>::get_value(_element, node);
  }

private:
  friend struct detail::ParseAccess;
  friend struct detail::ProbeAccess;
  friend struct detail::FastProbeAccess;
  friend struct detail::InitAccess;

  void init_impl(AstReflectionInitContext &ctx) const { parser::init(_element, ctx); }

  template <StrictParseModeContext Context>
  bool probe_impl(Context &ctx) const {
    return parser::probe(_element, ctx);
  }

  template <StrictParseModeContext Context>
  bool fast_probe_impl(Context &ctx) const {
    // `has_operator_fast_probe` sits on the strict infix hot path. When the
    // wrapped operator expression already exposes a specialized fast probe
    // (for example literals or ordered choices of literals), this avoids a
    // checkpoint for every operator lookahead.
    return parser::attempt_fast_probe(ctx, _element);
  }

  bool fast_probe_impl(RecoveryContext &ctx) const {
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
    std::unique_ptr<AstNode> (*getValue)(const void *, const CstNodeView &,
                                         std::unique_ptr<AstNode>,
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

  InfixRule super() const { return *this; }

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

  std::unique_ptr<AstNode>
  getValue(const CstNodeView &node,
           std::unique_ptr<AstNode> lhsNode,
           const ValueBuildContext &context) const override {
    assert(_obj && _ops.getValue && "Missing element wrapper!");
    return _ops.getValue(_obj, node, std::move(lhsNode), context);
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
  };

  template <NonNullableExpression Element, InfixOperatorExpression... Operators>
  struct Model {
    using Assoc = grammar::InfixOperator::Associativity;
    using Traits =
        detail::InfixRuleModelTraits<T, Left, Op, Right, Element, Operators...>;
    static constexpr std::int32_t kMaxPrecedence = Traits::kMaxPrecedence;
    using PrimaryType = typename Traits::PrimaryType;
    using LeftType = typename Traits::LeftType;
    using RightType = typename Traits::RightType;
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

    static std::unique_ptr<AstNode>
    getValue(const void *self, const CstNodeView &node,
             std::unique_ptr<AstNode> lhsNode,
             const ValueBuildContext &context) {
      return ValueBuilder::getValue(static_cast<const Model *>(self), node,
                                    std::move(lhsNode), context);
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
      const auto checkpoint = ctx.mark();
      const auto savedFurthestExploredCursor =
          ctx.furthestExploredCursor();
      const auto startOffset = ctx.cursorOffset();
      ctx.restoreFurthestExploredCursor(ctx.cursor());
      const bool matchedPrimary =
          parser::attempt_parse_no_edits(ctx, model->primary);
      const bool startedPrimary =
          matchedPrimary || ctx.furthestExploredOffset() > startOffset;
      ctx.rewind(checkpoint);
      ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
      return startedPrimary;
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
      };
    }

    inline static constexpr Ops opsTable = make_ops();

  private:
    struct ParseEngine {
      enum class EditableTailCandidateKind {
        ContinueTail,
        DeleteOperatorRun,
      };

      struct EditableTailCandidate {
        EditableTailCandidateKind kind =
            EditableTailCandidateKind::DeleteOperatorRun;
        std::uint32_t order = 0;
        std::uint32_t rhsPrefixNoiseDeleteCount = 0;
        bool legal = false;
      };

      struct EditableTailReplayResult {
        bool matched = true;
        bool stopTail = false;
      };

      struct EditableTailObservation {
        const char *rhsStart = nullptr;
        TextOffset rhsStartOffset = 0;
        std::size_t baseRecoveryEditCount = 0;
        std::uint32_t rhsPrefixNoiseDeleteCount = 0;
        bool exposedPrimary = true;
        bool matchedPrimary = false;
        bool advancedPrimary = false;
        bool crossedSkippedTrivia = false;
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
          const auto observation = observe_editable_rhs_primary(model, ctx);
          const auto candidates =
              enumerate_editable_tail_candidates(observation, ctx);
          const EditableTailCandidate *const selectedCandidate =
              select_editable_tail_candidate(candidates);
          if (selectedCandidate == nullptr) {
            ctx.rewind(nodeCheckpoint);
            return {.matched = false, .stopTail = false};
          }
          if (selectedCandidate->kind ==
              EditableTailCandidateKind::DeleteOperatorRun) {
            ctx.rewind(nodeCheckpoint);
            return recover_stray_operator_run(model, ctx, minPrecedence);
          }
          const auto replayResult = replay_editable_rhs_primary(
              model, ctx, *selectedCandidate, nextMinPrecedence, lhsStart);
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
          auto rhsNoEditGuard = ctx.withEditState(false, false, false);
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
        const bool previousAllowInsert = ctx.allowInsert;
        const bool previousAllowDelete = ctx.allowDelete;
        ctx.allowInsert = false;
        ctx.allowDelete = false;
        ctx.noteRecoveryPolicyMutation();
        const bool matched = parser::attempt_parse_editable(ctx, op);
        ctx.allowInsert = previousAllowInsert;
        ctx.allowDelete = previousAllowDelete;
        ctx.noteRecoveryPolicyMutation();
        if (matched) {
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
        const auto checkpoint = ctx.mark();
        const char *const savedFurthestExploredCursor =
            ctx.furthestExploredCursor();
        ctx.restoreFurthestExploredCursor(ctx.cursor());
        ctx.skip();
        if (has_operator_fast_probe(model, ctx, /*minPrecedence=*/0) &&
            !observe_rhs_prefix_operator_noise(model, ctx, observation)) {
          observation.exposedPrimary = false;
          ctx.rewind(checkpoint);
          ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
          return observation;
        }
        observation.rhsStart = ctx.cursor();
        observation.rhsStartOffset = ctx.cursorOffset();
        observation.baseRecoveryEditCount = ctx.recoveryEditCount();
        const bool previousAllowExtendedDeleteScan =
            ctx.allowExtendedDeleteScan;
        ctx.allowExtendedDeleteScan = false;
        ctx.noteRecoveryPolicyMutation();
        observation.matchedPrimary = parse(model->primary, ctx);
        ctx.allowExtendedDeleteScan = previousAllowExtendedDeleteScan;
        ctx.noteRecoveryPolicyMutation();
        observation.advancedPrimary = ctx.cursor() != observation.rhsStart;
        if (!observation.matchedPrimary || !observation.advancedPrimary) {
          ctx.rewind(checkpoint);
          ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
          return observation;
        }
        if (ctx.recoveryEditCount() == observation.baseRecoveryEditCount) {
          ctx.rewind(checkpoint);
          ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
          return observation;
        }
        const auto edits = ctx.recoveryEditsView();
        const auto immediateEditCount =
            ctx.recoveryEditCount() - observation.baseRecoveryEditCount;
        observation.crossedSkippedTrivia = rhs_primary_crossed_skipped_trivia(
            ctx, observation.rhsStart);
        observation.multipleImmediateEditsAtStart =
            immediateEditCount > 1u &&
            edits[observation.baseRecoveryEditCount].offset ==
                observation.rhsStartOffset;
        ctx.rewind(checkpoint);
        ctx.restoreFurthestExploredCursor(savedFurthestExploredCursor);
        return observation;
      }

      [[nodiscard]] static bool rhs_primary_crossed_skipped_trivia(
          RecoveryContext &ctx, const char *rhsStart) {
        const char *cursor = rhsStart;
        while (cursor < ctx.cursor()) {
          const char *const skipped = ctx.skip_without_builder(cursor);
          if (skipped > cursor) {
            return true;
          }
          ++cursor;
        }
        return false;
      }

      [[nodiscard]] static bool
      continue_editable_tail_candidate_is_legal(
          const EditableTailObservation &observation) noexcept {
        return observation.exposedPrimary && observation.matchedPrimary &&
               observation.advancedPrimary &&
               !observation.crossedSkippedTrivia &&
               !observation.multipleImmediateEditsAtStart;
      }

      [[nodiscard]] static std::array<EditableTailCandidate, 2>
      enumerate_editable_tail_candidates(const EditableTailObservation &observation,
                                         const RecoveryContext &ctx) noexcept {
        return {{
            {.kind = EditableTailCandidateKind::ContinueTail,
             .order = 0u,
             .rhsPrefixNoiseDeleteCount =
                 observation.rhsPrefixNoiseDeleteCount,
             .legal = continue_editable_tail_candidate_is_legal(observation)},
            {.kind = EditableTailCandidateKind::DeleteOperatorRun,
             .order = 1u,
             .legal = ctx.canDelete()},
        }};
      }

      [[nodiscard]] static const EditableTailCandidate *
      select_editable_tail_candidate(
          const std::array<EditableTailCandidate, 2> &candidates) noexcept {
        const EditableTailCandidate *bestCandidate = nullptr;
        for (const auto &candidate : candidates) {
          if (!candidate.legal) {
            continue;
          }
          if (bestCandidate == nullptr ||
              candidate.order < bestCandidate->order) {
            bestCandidate = std::addressof(candidate);
          }
        }
        return bestCandidate;
      }

      static EditableTailReplayResult
      replay_editable_rhs_primary(const Model *model, RecoveryContext &ctx,
                                  const EditableTailCandidate &candidate,
                                  std::int32_t nextMinPrecedence,
                                  const char *lhsStart) {
        assert(candidate.kind == EditableTailCandidateKind::ContinueTail &&
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
        const bool previousAllowExtendedDeleteScan =
            ctx.allowExtendedDeleteScan;
        ctx.allowExtendedDeleteScan = false;
        ctx.noteRecoveryPolicyMutation();
        const bool matchedPrimary = parse(model->primary, ctx);
        ctx.allowExtendedDeleteScan = previousAllowExtendedDeleteScan;
        ctx.noteRecoveryPolicyMutation();
        assert(matchedPrimary && ctx.cursor() != rhsStart &&
               "Observed editable infix RHS must replay successfully");
        if (!matchedPrimary || ctx.cursor() == rhsStart) {
          return {.matched = false, .stopTail = false};
        }
        ctx.skip();
        const auto nestedReplayResult =
            parse_tail_editable(model, ctx, nextMinPrecedence, rhsStart);
        assert(model->_owner && "The owner must be set");
        ctx.exit(lhsStart, model->_owner);
        return {.matched = true, .stopTail = nestedReplayResult.stopTail};
      }

      static EditableTailReplayResult
      recover_stray_operator_run(const Model *model, RecoveryContext &ctx,
                                 std::int32_t minPrecedence) {
        const bool previousSkipAfterDelete = ctx.skipAfterDelete;
        ctx.skipAfterDelete = false;
        ctx.noteRecoveryPolicyMutation();
        bool stopTail = false;
        const bool recovered =
            has_operator_fast_probe(model, ctx, minPrecedence) &&
            detail::recover_by_guarded_delete_scan(
                ctx,
                []() noexcept { return true; },
                [&](const char *scanCursor) -> const char * {
                  if (scanCursor == ctx.end ||
                      has_operator_fast_probe(model, ctx, minPrecedence)) {
                    return nullptr;
                  }
                  return scanCursor;
                },
                [&](const char *matchedCursor) {
                  stopTail =
                      ctx.skip_without_builder(matchedCursor) > matchedCursor;
                });
        ctx.skipAfterDelete = previousSkipAfterDelete;
        ctx.noteRecoveryPolicyMutation();
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
        const bool previousSkipAfterDelete = ctx.skipAfterDelete;
        ctx.skipAfterDelete = false;
        ctx.noteRecoveryPolicyMutation();
        const auto maxProbeDeletes =
            std::min<std::uint32_t>(ctx.maxConsecutiveCodepointDeletes, 4u);
        std::optional<std::uint32_t> observedDeleteCount;
        (void)detail::recover_by_guarded_delete_scan(
            ctx,
            [&]() noexcept {
              return ctx.cursor() != ctx.end &&
                     has_operator_fast_probe(model, ctx, /*minPrecedence=*/0);
            },
            [&](const char *) -> const char * {
              ctx.skip();
              if (parser::attempt_fast_probe(ctx, model->primary) ||
                  parser::probe_locally_recoverable(model->primary, ctx)) {
                return ctx.cursor();
              }
              return nullptr;
            },
            [&](const char *, std::uint32_t deleteCount) {
              observedDeleteCount = deleteCount;
            },
            {.maxDeletes = maxProbeDeletes, .allowOverflow = false});
        ctx.skipAfterDelete = previousSkipAfterDelete;
        ctx.noteRecoveryPolicyMutation();
        ctx.rewind(checkpoint);
        return observedDeleteCount;
      }

      static bool replay_rhs_prefix_operator_noise(const Model *model,
                                                   RecoveryContext &ctx,
                                                   std::uint32_t deleteCount) {
        if (deleteCount == 0u) {
          return true;
        }
        const bool previousSkipAfterDelete = ctx.skipAfterDelete;
        ctx.skipAfterDelete = false;
        ctx.noteRecoveryPolicyMutation();
        const bool replayed = detail::recover_by_guarded_delete_scan(
            ctx,
            [&]() noexcept {
              return has_operator_fast_probe(model, ctx, /*minPrecedence=*/0);
            },
            [&](const char *, std::uint32_t deletedCount) -> const char * {
              ctx.skip();
              return deletedCount == deleteCount ? ctx.cursor() : nullptr;
            },
            [](const char *) {},
            {.maxDeletes = deleteCount, .allowOverflow = false});
        ctx.skipAfterDelete = previousSkipAfterDelete;
        ctx.noteRecoveryPolicyMutation();
        return replayed;
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
