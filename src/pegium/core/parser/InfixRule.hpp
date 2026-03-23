#pragma once

/// Parser rule template implementing precedence-aware infix parsing.

#include <array>
#include <concepts>
#include <cstddef>
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
    bool (*recover)(const void *, RecoveryContext &);
    bool (*expect)(const void *, ExpectContext &);
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
      : _name(other._name), _obj(nullptr), _ops(other._ops) {
    if (other._obj)
      _obj = _ops.clone(other._obj);
  }

  InfixRule(InfixRule &&other) noexcept { move_from(std::move(other)); }

  InfixRule &operator=(const InfixRule &other) {
    if (this == &other)
      return *this;

    reset();

    _name = other._name;
    _ops = other._ops;

    if (other._obj)
      _obj = _ops.clone(other._obj);
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
private:
  friend struct detail::ParseAccess;
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

  void init_impl(AstReflectionInitContext &ctx) const {
    ctx.registerProducedType(detail::ast_node_type_info<T>());
    assert(_obj && _ops.init && "Missing infix init wrapper!");
    _ops.init(_obj, ctx);
  }

  void reset() noexcept {
    if (!_obj)
      return;
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
    static bool recover(const void *self, RecoveryContext &ctx) {
      return ParseEngine::recover(static_cast<const Model *>(self), ctx);
    }
    static bool expect(const void *self, ExpectContext &ctx) {
      return ParseEngine::expect(static_cast<const Model *>(self), ctx);
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
      return OperatorCatalog::getOperator(static_cast<const Model *>(self),
                                          index);
    }

    static std::size_t operatorCount(const void *) noexcept {
      return OperatorCatalog::operatorCount();
    }

    static void del(void *self) noexcept { delete static_cast<Model *>(self); }
    static void *clone(const void *self) {
      return new Model(*static_cast<const Model *>(self));
    }

    using Ops = InfixRule::Ops;

    static constexpr Ops make_ops() noexcept {
      return {
          .getValue = &Model::getValue,
          .rule = &Model::rule,
          .ruleTracked = &Model::ruleTracked,
          .recover = &Model::recover,
          .expect = &Model::expect,
          .init = &Model::init,
          .elem = &Model::elem,
          .getOperator = &Model::getOperator,
          .operatorCount = &Model::operatorCount,
          .destroy = &Model::del,
          .clone = &Model::clone,
      };
    }

    inline static constexpr Ops opsTable = make_ops();

  private:
    struct ParseEngine {
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
        return parse_tail_editable(model, ctx, /*minPrecedence=*/0,
                                   expressionStart);
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

      static bool parse_tail_editable(const Model *model, RecoveryContext &ctx,
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
          if (!try_match_operator_editable(model, ctx, minPrecedence,
                                           nextMinPrecedence)) {
            ctx.rewind(nodeCheckpoint);
            return true;
          }
          ctx.skip();
          const bool previousAllowInsert = ctx.allowInsert;
          const bool previousAllowDelete = ctx.allowDelete;
          const bool rhsStartsWithUnexpectedOperator =
              starts_with_operator_terminal(model, ctx.cursor());
          ctx.allowInsert = false;
          // Allow generic delete-based recovery inside the RHS primary only
          // when the tail starts with another operator token of the same infix
          // rule (for example `+` in `2 * +c`). This fixes doubled-operator
          // cases without letting the RHS delete arbitrary valid trailing
          // tokens such as statement terminators.
          ctx.allowDelete =
              previousAllowDelete && rhsStartsWithUnexpectedOperator;
          const auto rhsStart = ctx.cursor();
          const bool matchedRhs = parse(model->primary, ctx);
          ctx.allowInsert = previousAllowInsert;
          ctx.allowDelete = previousAllowDelete;
          if (!matchedRhs) {
            ctx.rewind(nodeCheckpoint);
            return true;
          }
          ctx.skip();
          (void)parse_tail_editable(model, ctx, nextMinPrecedence, rhsStart);
          assert(model->_owner && "The owner must be set");
          ctx.exit(lhsStart, model->_owner);
        }
      }

      template <std::size_t I = 0>
      static bool starts_with_operator_terminal(const Model *model,
                                                const char *cursor) noexcept {
        if constexpr (I == sizeof...(Operators)) {
          return false;
        } else {
          const auto &op = std::get<I>(model->ops);
          if constexpr (requires {
                          { op.terminal(cursor) } noexcept
                              -> std::same_as<const char *>;
                        }) {
            if (op.terminal(cursor) != nullptr) {
              return true;
            }
          }
          return starts_with_operator_terminal<I + 1>(model, cursor);
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

    };

  public:
    const grammar::InfixRule *_owner = nullptr;
    ExpressionHolder<Element> primary;
    std::tuple<Operators...> ops;
  };

  // ------------------- data -------------------
  std::string _name{};

  void *_obj = nullptr; // heap-only
  Ops _ops{};
};

} // namespace pegium::parser
