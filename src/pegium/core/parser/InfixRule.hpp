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
#include <pegium/core/parser/RecoveryConstants.hpp>
#include <pegium/core/parser/RecoveryUtils.hpp>
#include <pegium/core/parser/RecoveryTrace.hpp>
#include <pegium/core/parser/StepTrace.hpp>
#include <pegium/core/parser/AstReflectionBootstrap.hpp>
#include <pegium/core/parser/ValueBuildContext.hpp>
#include <string>
#include <type_traits>
#include <utility>

namespace pegium::parser::detail {

/// Closed list of `Infix` recovery candidate families. The 3 values are
/// exhaustive and frozen by the density-ceiling rule: a new family
/// requires removing or merging an existing one. This vocabulary is
/// private to `InfixRule` — no other combinator references it. Lives
/// inline in `InfixRule.hpp` (rather than its own header) because the
/// fan-out is exactly one combinator and there is no closed exhaustive
/// table that would justify exposing the type beyond the rule.
///
/// Strict-path discipline: this declaration is recovery-side. None of
/// these types are constructed on the strict-only nominal path.
enum class InfixCandidateFamily : std::uint8_t {
  /// The tail stops here: no strict operator at the cursor, no
  /// parent follow stronger than the tail. No local edit.
  StopTail,
  /// The tail continues: a strict operator at the cursor followed
  /// by RHS strict or recoverable acceptance.
  ContinueTail,
  /// The tail recovers from operator-position noise: a bounded
  /// delete of pre-operator junk, after which a strict RHS primary
  /// starts.
  DeleteOperatorNoise,
};

/// Closed structural facts the legality predicates of
/// `InfixCandidateFamily` consume. Each predicate reads only a
/// subset; the bundle exists so callers fill it once. Adding a fact
/// here must come with its consumer (one of the legality
/// predicates) and a test pinning the new dependency.
struct InfixCandidateFamilyFacts {
  /// True iff a strict operator accepts at the current cursor.
  bool strictOperatorAtCursor = false;

  /// True iff a recoverable operator accepts at the current cursor
  /// (typically a fuzzy operator match).
  bool recoverableOperatorAtCursor = false;

  /// True iff the parent's strict follow accepts at the current
  /// cursor and would prefer the expression terminate.
  bool parentFollowStrict = false;

  /// True iff a strict RHS primary accepts immediately after the
  /// operator. Necessary for `ContinueTail`.
  bool strictRhsPrimaryAfterOperator = false;

  /// True iff a recoverable RHS primary accepts after the operator.
  /// Sufficient for `ContinueTail` when strict does not apply.
  bool recoverableRhsPrimaryAfterOperator = false;

  /// True iff a strict RHS primary accepts after a bounded
  /// operator-noise delete scan. Required for
  /// `DeleteOperatorNoise`. Without this, deleting noise is not
  /// admissible (the scan would not commit to a real candidate).
  bool strictRhsPrimaryAfterNoiseDelete = false;

  /// True iff the operator-noise delete budget is non-zero
  /// (`maxConsecutiveCodepointDeletes > consecutiveDeletes`).
  bool operatorNoiseDeleteBudgetAvailable = false;

  [[nodiscard]] friend bool
  operator==(const InfixCandidateFamilyFacts &a,
             const InfixCandidateFamilyFacts &b) noexcept = default;
};

static_assert(std::is_trivially_copyable_v<InfixCandidateFamilyFacts>);
static_assert(sizeof(InfixCandidateFamilyFacts) <= 8);

/// Closed legality predicate for `StopTail`. The tail stops when
/// no strict operator accepts at the cursor — there is nothing to
/// continue with. Recoverable operators alone are not enough:
/// operators are never synthesised, so a recoverable-only operator
/// cannot drive `ContinueTail` either; in such a case `StopTail` is
/// still the admissible default.
[[nodiscard]] constexpr bool
is_stop_tail_legal(const InfixCandidateFamilyFacts &facts) noexcept {
  return !facts.strictOperatorAtCursor;
}

/// Closed legality predicate for `ContinueTail`. Continuation
/// requires a strict operator at the cursor (operators are never
/// synthesised) and a RHS primary that accepts strictly or
/// recoverably immediately after.
[[nodiscard]] constexpr bool
is_continue_tail_legal(const InfixCandidateFamilyFacts &facts) noexcept {
  return facts.strictOperatorAtCursor &&
         (facts.strictRhsPrimaryAfterOperator ||
          facts.recoverableRhsPrimaryAfterOperator);
}

/// Closed legality predicate for `DeleteOperatorNoise`. The scan is
/// refused if no strict primary follows: the noise delete is
/// admissible only when a strict RHS primary starts after the scan
/// AND the budget allows the scan. Crucially this predicate requires
/// the FOLLOWING strict primary, not the preceding strict operator:
/// the scan deletes noise that prevents the operator from being
/// seen.
[[nodiscard]] constexpr bool
is_delete_operator_noise_legal(
    const InfixCandidateFamilyFacts &facts) noexcept {
  return facts.strictRhsPrimaryAfterNoiseDelete &&
         facts.operatorNoiseDeleteBudgetAvailable;
}

/// Closed dispatch over the legality predicates. Returns true iff
/// the family is admissible under the given facts.
[[nodiscard]] constexpr bool
is_infix_candidate_family_legal(
    InfixCandidateFamily family,
    const InfixCandidateFamilyFacts &facts) noexcept {
  switch (family) {
  case InfixCandidateFamily::StopTail:
    return is_stop_tail_legal(facts);
  case InfixCandidateFamily::ContinueTail:
    return is_continue_tail_legal(facts);
  case InfixCandidateFamily::DeleteOperatorNoise:
    return is_delete_operator_noise_legal(facts);
  }
  return false;
}

/// Returns a short stable identifier for the family.
[[nodiscard]] constexpr const char *
infix_candidate_family_name(InfixCandidateFamily family) noexcept {
  switch (family) {
  case InfixCandidateFamily::StopTail:
    return "StopTail";
  case InfixCandidateFamily::ContinueTail:
    return "ContinueTail";
  case InfixCandidateFamily::DeleteOperatorNoise:
    return "DeleteOperatorNoise";
  }
  return "Unknown";
}

/// `LocalRhsNoiseCleanup`: closed local contract that disciplines
/// the RHS noise cleanup inside `Infix`. NOT a selection axis;
/// the dispatch must read this contract only as a check on the
/// existing observation/admission/replay obligations.
///
/// The contract carries three flags:
///
///   - `respectsObservationObligation`: the noise scan observes
///     without durable mutation (checkpoint/rewind covers all
///     state mutated by the scan). Required: `true`.
///   - `respectsReplayObligation`: the candidate produced by the
///     scan replays the same noise edits exactly. Required:
///     `true`.
///   - `policyMutationVisibleInFingerprint`: any policy field the
///     scan flips (for instance `allowDelete`) is folded into the
///     `RecoveryPolicyFingerprint` so the cache cannot collapse a
///     hit across the flipped state. Required: `true`.
///
/// Construction outside `Infix` is forbidden — `LocalRhsNoiseCleanup`
/// cannot be referenced by another combinator or by
/// `RecoveryContract`. The legality predicate
/// `is_local_rhs_noise_cleanup_admissible` lets `Infix` assert that a
/// candidate respects the contract before it enters the pipeline.
struct LocalRhsNoiseCleanup {
  bool respectsObservationObligation = false;
  bool respectsReplayObligation = false;
  bool policyMutationVisibleInFingerprint = false;

  [[nodiscard]] friend bool
  operator==(const LocalRhsNoiseCleanup &a,
             const LocalRhsNoiseCleanup &b) noexcept = default;
};

static_assert(std::is_trivially_copyable_v<LocalRhsNoiseCleanup>);
static_assert(sizeof(LocalRhsNoiseCleanup) <= 4);

/// True iff the contract has all three obligations satisfied.
/// `Infix`'s noise cleanup MUST verify this before admitting a
/// `DeleteOperatorNoise` candidate.
[[nodiscard]] constexpr bool
is_local_rhs_noise_cleanup_admissible(
    const LocalRhsNoiseCleanup &contract) noexcept {
  return contract.respectsObservationObligation &&
         contract.respectsReplayObligation &&
         contract.policyMutationVisibleInFingerprint;
}

/// RAII guard that disables `skipAfterDelete` for the duration of an
/// `InfixRule` stray-operator-run probe. Without this flip, the bounded
/// delete scan would absorb a hidden separator that crosses a logical
/// boundary, and the scan would commit to a wrong run length. The
/// mutation is observed by `RecoveryPolicyFingerprint` so the recovery
/// cache cannot collapse a hit across the flipped state.
struct [[nodiscard]] ScopedSkipAfterDeleteDisabled {
  RecoveryContext &ctx;
  bool savedSkipAfterDelete;

  explicit ScopedSkipAfterDeleteDisabled(RecoveryContext &c) noexcept
      : ctx(c), savedSkipAfterDelete(c.skipAfterDelete) {
    ctx.skipAfterDelete = false;
  }
  ~ScopedSkipAfterDeleteDisabled() noexcept {
    ctx.skipAfterDelete = savedSkipAfterDelete;
  }
  ScopedSkipAfterDeleteDisabled(const ScopedSkipAfterDeleteDisabled &) = delete;
  ScopedSkipAfterDeleteDisabled &
  operator=(const ScopedSkipAfterDeleteDisabled &) = delete;
  ScopedSkipAfterDeleteDisabled(ScopedSkipAfterDeleteDisabled &&) = delete;
  ScopedSkipAfterDeleteDisabled &
  operator=(ScopedSkipAfterDeleteDisabled &&) = delete;
};

/// RAII guard for the `InfixRule` RHS noise-cleanup scope. Disables
/// `skipAfterDelete` AND enables `allowDelete` for the duration of an
/// observation/replay probe. The `allowDelete=true` flip is what lets
/// the bounded scan run even when an outer `InsertRetry` parent
/// disabled deletes globally — without it, the probe would fail and
/// the outer driver would fabricate multi-edit paths that bypass
/// `InfixRule`'s clean local fix. Both mutations are observed by
/// `RecoveryPolicyFingerprint` so the recovery cache cannot collapse
/// a hit across the flipped state. This RAII is the only place the
/// `LocalRhsNoiseCleanup` policy mutation is realised.
struct [[nodiscard]] ScopedInfixNoiseCleanupPolicy {
  RecoveryContext &ctx;
  bool savedSkipAfterDelete;
  bool savedAllowDelete;

  explicit ScopedInfixNoiseCleanupPolicy(RecoveryContext &c) noexcept
      : ctx(c), savedSkipAfterDelete(c.skipAfterDelete),
        savedAllowDelete(c.allowDelete) {
    ctx.skipAfterDelete = false;
    ctx.allowDelete = true;
  }
  ~ScopedInfixNoiseCleanupPolicy() noexcept {
    ctx.skipAfterDelete = savedSkipAfterDelete;
    ctx.allowDelete = savedAllowDelete;
  }
  ScopedInfixNoiseCleanupPolicy(const ScopedInfixNoiseCleanupPolicy &) = delete;
  ScopedInfixNoiseCleanupPolicy &
  operator=(const ScopedInfixNoiseCleanupPolicy &) = delete;
  ScopedInfixNoiseCleanupPolicy(ScopedInfixNoiseCleanupPolicy &&) = delete;
  ScopedInfixNoiseCleanupPolicy &
  operator=(ScopedInfixNoiseCleanupPolicy &&) = delete;
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

  /// Per-call upper bound on temporary recovery candidates evaluated
  /// inside one editable-tail recovery cycle.
  /// `select_editable_tail_candidate_by_recovery_key` iterates a
  /// `std::array<EditableTailCandidate, 2>` (one for `ContinueTail`,
  /// one for `DeleteOperatorNoise`) and selects via the central
  /// `RecoveryKey` ranker. The bound is the static array size of 2.
  /// The `StopTail` family is admission-only (it accepts the current
  /// state without a candidate evaluation), so it is not counted.
  /// The bound is independent of operator arity, expression depth,
  /// and input length.
  static constexpr std::size_t kMaxRecoveryCandidatesPerCall = 2U;

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
      detail::ProbeRestoreScope guard{ctx};
      const auto startOffset = ctx.cursorOffset();
      ctx.restoreFurthestExploredCursor(ctx.cursor());
      const bool matchedPrimary =
          parser::attempt_parse_no_edits(ctx, model->primary);
      return matchedPrimary || ctx.furthestExploredOffset() > startOffset;
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
      struct EditableTailCandidate {
        detail::InfixCandidateFamily kind =
            detail::InfixCandidateFamily::DeleteOperatorNoise;
        std::uint32_t rhsPrefixNoiseDeleteCount = 0;
        bool rhsPrimaryObservedWithoutEdits = false;
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
        // `RecoveryKey` for the `DeleteOperatorNoise` candidate
        // instead of the legacy single-delete cost estimate. Empty
        // when the scan would not admit a recovery from this cursor.
        std::optional<std::uint32_t> strayOperatorRunDeleteCount;
        TextOffset strayOperatorRunFirstEditOffset = 0;
        bool exposedPrimary = true;
        bool matchedPrimary = false;
        bool advancedPrimary = false;
        bool rhsPrimaryObservedWithoutEdits = false;
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
          ctx.skip();
          const auto rhsStart = ctx.cursor();
          const char *const directRhsSavedFurthest =
              ctx.furthestExploredCursor();
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
          ctx.restoreFurthestExploredCursor(directRhsSavedFurthest);
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
              enumerate_editable_tail_candidates(observation, ctx);
          const auto selection =
              select_editable_tail_candidate_by_recovery_key(candidates,
                                                             observation);
          if (selection.selected == nullptr) {
            ctx.rewind(nodeCheckpoint);
            return {.matched = false, .stopTail = false};
          }
          if (selection.selected->kind ==
              detail::InfixCandidateFamily::DeleteOperatorNoise) {
            ctx.rewind(nodeCheckpoint);
            return recover_stray_operator_run(model, ctx, minPrecedence);
          }
          const auto replayResult = replay_editable_rhs_primary(
              model, ctx, *selection.selected, nextMinPrecedence, lhsStart);
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
        if (!parser::attempt_fast_probe(ctx, op)) {
          if constexpr (I + 1 == sizeof...(Operators)) {
            return false;
          } else {
            return try_match_operator_editable<I + 1>(model, ctx, minPrecedence,
                                                      nextMinPrecedence);
          }
        }
        const bool previousAllowInsert = ctx.allowInsert;
        const bool previousAllowDelete = ctx.allowDelete;
        ctx.allowInsert = false;
        ctx.allowDelete = false;
        const bool matched = parser::attempt_parse_editable(ctx, op);
        ctx.allowInsert = previousAllowInsert;
        ctx.allowDelete = previousAllowDelete;
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
        detail::ProbeRestoreScope guard{ctx};
        ctx.restoreFurthestExploredCursor(ctx.cursor());
        ctx.skip();
        if (has_operator_fast_probe(model, ctx, /*minPrecedence=*/0) &&
            !observe_rhs_prefix_operator_noise(model, ctx, observation)) {
          observation.exposedPrimary = false;
          return observation;
        }
        observation.rhsStart = ctx.cursor();
        observation.rhsStartOffset = ctx.cursorOffset();
        observation.baseRecoveryEditCount = ctx.recoveryEditCount();
        {
          detail::ProbeRestoreScope noEditGuard{ctx};
          if (parser::attempt_parse_no_edits(ctx, model->primary) &&
              ctx.cursor() != observation.rhsStart) {
            observation.matchedPrimary = true;
            observation.advancedPrimary = true;
            observation.rhsPrimaryObservedWithoutEdits = true;
            observation.postMatchCursorOffset = ctx.cursorOffset();
            return observation;
          }
        }
        const bool previousAllowExtendedDeleteScan =
            ctx.allowExtendedDeleteScan;
        ctx.allowExtendedDeleteScan = false;
        observation.matchedPrimary = parse(model->primary, ctx);
        ctx.allowExtendedDeleteScan = previousAllowExtendedDeleteScan;
        observation.advancedPrimary = ctx.cursor() != observation.rhsStart;
        observation.postMatchCursorOffset = ctx.cursorOffset();
        if (!observation.matchedPrimary || !observation.advancedPrimary) {
          return observation;
        }
        if (ctx.recoveryEditCount() == observation.baseRecoveryEditCount) {
          return observation;
        }
        const auto edits = ctx.recoveryEditsView();
        observation.immediateEditCount = static_cast<std::uint32_t>(
            ctx.recoveryEditCount() - observation.baseRecoveryEditCount);
        observation.crossedSkippedTrivia = rhs_primary_crossed_skipped_trivia(
            ctx, observation.rhsStart);
        observation.multipleImmediateEditsAtStart =
            observation.immediateEditCount > 1u &&
            edits[observation.baseRecoveryEditCount].offset ==
                observation.rhsStartOffset;
        // Capture the first-edit offset and the aggregate edit cost so
        // the caller can project the ContinueTail candidate onto a
        // `RecoveryKey` after the rewind has rolled back the edits.
        observation.firstImmediateEditOffset =
            edits[observation.baseRecoveryEditCount].offset;
        for (std::size_t i = observation.baseRecoveryEditCount;
             i < ctx.recoveryEditCount(); ++i) {
          observation.immediateEditCost +=
              detail::default_edit_cost(edits[i].kind);
        }
        // InfixRule tail should match `many(op + Primary)` semantics: the
        // rhs primary is matched *starting at rhsStart*. If the speculation
        // committed any edit *before* rhsStart, it back-edited an earlier
        // token of the input to reshape it (e.g. inserting `(` at offset
        // 23 to reinterpret `c-4+;` as `c(-4)+;`). That is a grammar
        // reinterpretation, not a local rhs match, and it routinely
        // produces multi-edit speculative cascades that the outer
        // recovery cannot compete with. Treat such observations as
        // rejecting the ContinueTail candidate.
        if (observation.firstImmediateEditOffset <
            observation.rhsStartOffset) {
          observation.multipleImmediateEditsAtStart = true;
        }
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
        const bool continuationObserved =
            continue_editable_tail_candidate_is_legal(observation);
        detail::InfixCandidateFamilyFacts facts;
        facts.strictOperatorAtCursor = true;
        facts.recoverableOperatorAtCursor = true;
        facts.strictRhsPrimaryAfterOperator =
            continuationObserved && observation.immediateEditCount == 0u;
        facts.recoverableRhsPrimaryAfterOperator = continuationObserved;
        facts.strictRhsPrimaryAfterNoiseDelete =
            observation.strayOperatorRunDeleteCount.has_value();
        // The delete-noise scan rewinds to the pre-operator-match cursor
        // before replaying. Using `ctx.canDelete()` here would check the
        // post-operator-match cursor and miss cases where a committed
        // delete from a prior window is replayable at the operator
        // position but the post-op cursor sits below the new window's
        // editFloor. The successful observation already proved the budget
        // was available at the correct cursor.
        facts.operatorNoiseDeleteBudgetAvailable =
            observation.strayOperatorRunDeleteCount.has_value();

        const detail::LocalRhsNoiseCleanup noiseCleanup{
            .respectsObservationObligation =
                observation.strayOperatorRunDeleteCount.has_value(),
            .respectsReplayObligation =
                observation.strayOperatorRunDeleteCount.has_value(),
            .policyMutationVisibleInFingerprint = true,
        };
        const bool continueTailLegal =
            continuationObserved &&
            detail::is_infix_candidate_family_legal(
                detail::InfixCandidateFamily::ContinueTail, facts);
        const bool deleteNoiseLegal =
            detail::is_infix_candidate_family_legal(
                detail::InfixCandidateFamily::DeleteOperatorNoise, facts) &&
            detail::is_local_rhs_noise_cleanup_admissible(noiseCleanup);

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

      struct EditableTailCandidateSelection {
        const EditableTailCandidate *selected = nullptr;
      };

      /// Project each legal candidate onto the shared `RecoveryKey` and
      /// select the best — the same comparator used across the rest of
      /// recovery.
      ///
      /// ContinueTail's key comes from the observation (already computed,
      /// costs captured before the speculative rewind).
      ///
      /// DeleteOperatorNoise's key comes from the bounded, rewound
      /// observation. The winner path still replays the scan before
      /// committing edits, so observation and replay stay separate.
      [[nodiscard]] static EditableTailCandidateSelection
      select_editable_tail_candidate_by_recovery_key(
          const std::array<EditableTailCandidate, 2> &candidates,
          const EditableTailObservation &observation) {
        EditableTailCandidateSelection selection;
        detail::RecoveryKey bestKey;
        bool anySelected = false;

        const auto considerCandidate = [&](const EditableTailCandidate &cand,
                                           const detail::RecoveryKey &key) {
          if (!anySelected ||
              detail::is_better_recovery_key(key, bestKey)) {
            anySelected = true;
            bestKey = key;
            selection.selected = std::addressof(cand);
          }
        };

        for (const auto &candidate : candidates) {
          if (!candidate.legal) {
            continue;
          }
          if (candidate.kind == detail::InfixCandidateFamily::ContinueTail) {
            const auto effectiveFirstEdit =
                observation.immediateEditCount == 0u
                    ? observation.postMatchCursorOffset
                    : observation.firstImmediateEditOffset;
            const detail::RecoveryKey key{
                .matched = observation.matchedPrimary,
                .firstEditOffset = effectiveFirstEdit,
                .editCost = observation.immediateEditCost,
                .progressAfterEdits = observation.postMatchCursorOffset,
            };
            considerCandidate(candidate, key);
          } else {
            // Real DeleteOperatorNoise cost from the observable scan
            // (`observe_stray_operator_run_delete_count`). Replaces the
            // legacy single-delete estimate so the candidate's
            // `RecoveryKey` reflects the actual codepoint count the
            // committed `recover_stray_operator_run` would produce.
            //
            // When the observable scan reports `nullopt`, the scan
            // would not admit a recovery from the pre-operator-match
            // cursor; the candidate is structurally inadmissible and
            // we skip it (replaces the implicit "always-considered"
            // contract that the legacy estimate carried).
            if (!observation.strayOperatorRunDeleteCount.has_value()) {
              continue;
            }
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
                .progressAfterEdits = observation.rhsStartOffset,
            };
            considerCandidate(candidate, key);
          }
        }
        return selection;
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
          const bool previousAllowExtendedDeleteScan =
              ctx.allowExtendedDeleteScan;
          ctx.allowExtendedDeleteScan = false;
          matchedPrimary = parse(model->primary, ctx);
          ctx.allowExtendedDeleteScan = previousAllowExtendedDeleteScan;
        }
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

      /// Observable variant of `recover_stray_operator_run`: runs the
      /// same delete-scan logic against the cursor without committing
      /// any mutation (cursor, skipAfterDelete, edits all rewound), and
      /// returns the number of codepoint deletes that the scan would
      /// produce on commit. `std::nullopt` means the scan would not
      /// admit a `DeleteOperatorNoise` recovery from the current cursor
      /// (operator probe fails or the scan runs past its budget without
      /// matching). Used to project a real `RecoveryKey` for the
      /// `DeleteOperatorNoise` candidate at selection time, replacing
      /// the legacy single-delete cost estimate.
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
          detail::ScopedSkipAfterDeleteDisabled skipPolicy{ctx};
          const auto hidesRecoverablePrimaryBehindLocalGap = [&]() {
            detail::ProbeRestoreScope localGuard{ctx};
            const char *const gapStart = ctx.cursor();
            const char *const gapEnd = ctx.skip_without_builder(gapStart);
            ctx.skip();
            return gapEnd > gapStart + 1 && ctx.cursor() < ctx.end &&
                   (parser::attempt_fast_probe(ctx, model->primary) ||
                    parser::probe_locally_recoverable(model->primary, ctx));
          };
          (void)detail::recover_by_guarded_delete_scan(
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
              [&](const char *, std::uint32_t deleteCount) {
                capturedDeleteCount = deleteCount;
              });
        }
        return capturedDeleteCount;
      }

      static EditableTailReplayResult
      recover_stray_operator_run(const Model *model, RecoveryContext &ctx,
                                 std::int32_t minPrecedence) {
        detail::ScopedSkipAfterDeleteDisabled skipPolicy{ctx};
        const auto hidesRecoverablePrimaryBehindLocalGap = [&]() {
          detail::ProbeRestoreScope guard{ctx};
          const char *const gapStart = ctx.cursor();
          const char *const gapEnd = ctx.skip_without_builder(gapStart);
          ctx.skip();
          return gapEnd > gapStart + 1 && ctx.cursor() < ctx.end &&
                 (parser::attempt_fast_probe(ctx, model->primary) ||
                  parser::probe_locally_recoverable(model->primary, ctx));
        };
        bool stopTail = false;
        const bool recovered =
            has_operator_fast_probe(model, ctx, minPrecedence) &&
            detail::recover_by_guarded_delete_scan(
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
                [&](const char *matchedCursor) {
                  stopTail =
                      ctx.skip_without_builder(matchedCursor) > matchedCursor;
                });
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
          // ScopedInfixNoiseCleanupPolicy realises the LocalRhsNoiseCleanup
          // contract: skipAfterDelete=false (so the bounded scan does not
          // absorb a hidden separator) AND allowDelete=true (so the probe
          // works even when an outer InsertRetry parent disabled deletes
          // — InfixRule's RHS noise delete is a grammar-driven affordance
          // of the Pratt pattern, bounded by
          // `kInfixOperatorNoiseObservationDeleteCap`, not free delete
          // permission). Both mutations are visible in
          // `RecoveryPolicyFingerprint`.
          detail::ScopedInfixNoiseCleanupPolicy policy{ctx};
          const auto maxProbeDeletes =
              std::min(ctx.maxConsecutiveCodepointDeletes,
                       kInfixOperatorNoiseObservationDeleteCap);
          (void)detail::recover_by_guarded_delete_scan(
              ctx,
              [&]() noexcept {
                return ctx.cursor() != ctx.end &&
                       has_operator_fast_probe(model, ctx, /*minPrecedence=*/0);
              },
              [&](const char *) -> const char * {
                ctx.skip();
                // Strict probe only: the purpose of the scan is to find a
                // position where `primary` strictly starts. Allowing
                // `probe_locally_recoverable` here would stop after a
                // single delete on any run of operator-like noise (the
                // recoverable probe is too permissive at operator
                // positions), leaving the remaining stray operators
                // unhandled by ContinueTail.
                if (parser::attempt_fast_probe(ctx, model->primary)) {
                  return ctx.cursor();
                }
                return nullptr;
              },
              [&](const char *, std::uint32_t deleteCount) {
                observedDeleteCount = deleteCount;
              },
              {.maxDeletes = maxProbeDeletes, .allowOverflow = false});
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
        detail::ScopedInfixNoiseCleanupPolicy policy{ctx};
        return detail::recover_by_guarded_delete_scan(
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
