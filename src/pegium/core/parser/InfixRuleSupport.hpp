#pragma once

/// Support helpers for parser-side infix rule reduction.

#include <array>
#include <concepts>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <pegium/core/grammar/InfixRule.hpp>
#include <pegium/core/parser/ExpectContext.hpp>
#include <pegium/core/parser/ExpectFrontier.hpp>
#include <pegium/core/parser/ParseAttempt.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/ParseExpression.hpp>
#include <pegium/core/parser/RawValueTraits.hpp>
#include <pegium/core/parser/ValueBuildContext.hpp>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace pegium::parser {

template <NonNullableExpression Element,
          grammar::InfixOperator::Associativity Assoc>
struct InfixOperator;

} // namespace pegium::parser

namespace pegium::parser::detail {

template <typename T>
constexpr decltype(auto) move_if_owned(T &value) noexcept {
  if constexpr (std::is_reference_v<T>) {
    return value;
  } else {
    return std::move(value);
  }
}

template <typename T, auto Left, auto Op, auto Right, typename Element,
          typename... Operators>
struct InfixRuleModelTraits {
  static constexpr std::int32_t kMaxPrecedence =
      static_cast<std::int32_t>(sizeof...(Operators));
  using PrimaryType = typename std::remove_cvref_t<Element>::type;
  using LeftType =
      std::remove_reference_t<decltype(std::declval<T &>().*Left)>;
  using RightType =
      std::remove_reference_t<decltype(std::declval<T &>().*Right)>;
  using OpType = std::remove_reference_t<decltype(std::declval<T &>().*Op)>;
  static_assert(std::is_pointer_v<LeftType>,
                "InfixRule Left member must be a raw pointer (T *).");
  static_assert(std::is_pointer_v<RightType>,
                "InfixRule Right member must be a raw pointer (T *).");
  using LeftPointeeType = std::remove_pointer_t<LeftType>;
  using RightPointeeType = std::remove_pointer_t<RightType>;

  static_assert(sizeof...(Operators) > 0,
                "InfixRule requires at least one operator.");
  static_assert(std::derived_from<LeftPointeeType, AstNode>,
                "InfixRule Left member must point to an AstNode type.");
  static_assert(std::derived_from<RightPointeeType, AstNode>,
                "InfixRule Right member must point to an AstNode type.");
  static_assert(std::derived_from<PrimaryType, LeftPointeeType>,
                "InfixRule primary rule type must be assignable to Left.");
  static_assert(std::derived_from<PrimaryType, RightPointeeType>,
                "InfixRule primary rule type must be assignable to Right.");
  static_assert(std::derived_from<T, LeftPointeeType>,
                "InfixRule recursive node type must be assignable to Left.");
  static_assert(std::derived_from<T, RightPointeeType>,
                "InfixRule recursive node type must be assignable to Right.");

  template <typename RawValue>
  static constexpr bool operator_raw_assignable =
      std::same_as<std::remove_cvref_t<RawValue>, OpType> ||
      std::is_constructible_v<OpType, RawValue> ||
      (std::is_enum_v<OpType> &&
       std::is_integral_v<std::remove_cvref_t<RawValue>>) ||
      std::same_as<OpType, std::string>;

  template <typename RawValue>
  struct OperatorRawAssignable
      : std::bool_constant<operator_raw_assignable<RawValue>> {};

  template <typename OperatorExpr> struct OperatorCompatible : std::false_type {};

  template <NonNullableExpression OperatorElement,
            grammar::InfixOperator::Associativity OperatorAssoc>
  struct OperatorCompatible<InfixOperator<OperatorElement, OperatorAssoc>>
      : std::bool_constant<
            detail::expression_raw_compliant_v<OperatorElement,
                                               OperatorRawAssignable>> {};

  static constexpr bool operatorsCompatible =
      (OperatorCompatible<std::remove_cvref_t<Operators>>::value && ...);

  static_assert(
      operatorsCompatible,
      "Every InfixRule operator must expose a specific raw value that can be "
      "assigned to the Op member.");
};

template <typename Element> struct InfixOperatorValueSupport {
  template <typename Visitor>
  static bool visit_raw_value(const Element &element, const CstNodeView &node,
                              const ValueBuildContext &context,
                              Visitor &&visitor) {
    if constexpr (IsOrderedChoice<Element>::value) {
      for (const auto view : node) {
        const auto &child = view.node();
        if (child.isHidden) {
          continue;
        }

        const auto *grammarElement = child.grammarElement;
        assert(grammarElement && "Node must have a grammar element");
        return detail::visit_selected_ordered_choice_raw_value(
            element, grammarElement, view, context, visitor);
      }
      return false;
    } else {
      visitor(detail::extract_raw_value(element, node, context));
      return true;
    }
  }
};

template <typename Model, typename NodeType, auto Left, auto Op, auto Right,
          typename OpType, typename LeftPointeeType, typename RightPointeeType,
          typename... Operators>
struct InfixRuleValueBuilder {
  static AstNode *getValue(const Model *model, const CstNodeView &node,
                           AstNode *lhsNode,
                           const ValueBuildContext &context) {
    return cast_operand<AstNode>(buildTypedFromCst(
        model, node, cast_operand<LeftPointeeType>(lhsNode), context));
  }

private:
  template <typename Target, typename Source>
  static Target *cast_operand(Source *value) {
    static_assert(std::derived_from<Source, AstNode>,
                  "InfixRule operands must derive from AstNode.");
    static_assert(std::derived_from<Target, AstNode>,
                  "InfixRule target operands must derive from AstNode.");
    assert(value != nullptr);
    auto *astNode = static_cast<AstNode *>(value);
    return static_cast<Target *>(astNode);
  }

  template <typename RawValue>
  static bool assign_operator_raw(NodeType *node, RawValue &&value) {
    using ValueType = std::remove_cvref_t<RawValue>;
    if constexpr (std::same_as<ValueType, OpType> ||
                  std::is_constructible_v<OpType, RawValue>) {
      node->*Op = OpType{std::forward<RawValue>(value)};
      return true;
    } else if constexpr (std::is_enum_v<OpType> &&
                         std::is_integral_v<ValueType>) {
      node->*Op = static_cast<OpType>(value);
      return true;
    } else if constexpr (std::same_as<OpType, std::string>) {
      if constexpr (std::same_as<ValueType, std::string>) {
        node->*Op = std::forward<RawValue>(value);
      } else if constexpr (std::same_as<ValueType, std::string_view>) {
        node->*Op = std::string(value);
      } else if constexpr (std::same_as<ValueType, char>) {
        node->*Op = std::string(1, value);
      } else if constexpr (std::same_as<ValueType, bool>) {
        node->*Op = value ? "true" : "false";
      } else if constexpr (std::same_as<ValueType, std::nullptr_t>) {
        node->*Op = std::string{};
      } else {
        node->*Op = std::to_string(value);
      }
      return true;
    } else {
      return false;
    }
  }

  template <typename OperatorExpr>
  static bool assign_operator_from_expression(
      NodeType *node, const OperatorExpr &operatorExpression,
      const CstNodeView &operatorNode, const ValueBuildContext &context) {
    bool assigned = false;
    const bool visited = operatorExpression.visitRawValue(
        operatorNode, context,
        [&assigned, node]<typename RawValue>(RawValue &&rawValue) {
          assigned = assign_operator_raw(node, std::forward<RawValue>(rawValue));
        });
    assert(visited && "Infix operator node must yield a raw value.");
    assert(assigned && "Infix operator raw value is not assignable to Op.");
    return visited && assigned;
  }

  template <std::size_t I = 0>
  static bool assign_operator_from_node(const Model *model, NodeType *node,
                                        const CstNodeView &operatorNode,
                                        const ValueBuildContext &context) {
    if constexpr (I == sizeof...(Operators)) {
      return false;
    } else {
      if (const auto &operatorExpression = std::get<I>(model->ops);
          operatorNode.getGrammarElement() ==
          std::addressof(operatorExpression)) {
        return assign_operator_from_expression(node, operatorExpression,
                                               operatorNode, context);
      }
      return assign_operator_from_node<I + 1>(model, node, operatorNode,
                                              context);
    }
  }

  static NodeType *buildTypedFromCst(const Model *model,
                                     const CstNodeView &node,
                                     LeftPointeeType *lhsNode,
                                     const ValueBuildContext &context) {
    RightPointeeType *rhs = nullptr;
    CstNodeView operatorNode;
    for (const auto child : node) {
      if (child.node().isHidden) {
        continue;
      }
      if (!operatorNode.valid()) {
        operatorNode = child;
        continue;
      }
      if (rhs == nullptr) {
        assert(child.getGrammarElement() == std::addressof(model->primary));
        rhs = cast_operand<RightPointeeType>(
            detail::extract_raw_value(model->primary, child, context));
        continue;
      }
      assert(child.getGrammarElement() == model->_owner);
      rhs = cast_operand<RightPointeeType>(
          buildTypedFromCst(model, child, cast_operand<LeftPointeeType>(rhs),
                            context));
    }

    assert(lhsNode != nullptr);
    assert(rhs != nullptr);
    assert(operatorNode.valid());
    return createNode(model, node, lhsNode, operatorNode, rhs, context);
  }

  static NodeType *createNode(const Model *model, const CstNodeView &cstNode,
                              LeftPointeeType *left,
                              const CstNodeView &operatorNode,
                              RightPointeeType *right,
                              const ValueBuildContext &context) {
    assert(context.arena != nullptr);
    auto *node = context.arena->template create<NodeType>();
    node->setCstNode(cstNode);

    auto &leftMember = node->*Left;
    leftMember = left;
    leftMember->setContainer(*node);

    const bool assignedOperator =
        assign_operator_from_node(model, node, operatorNode, context);
    assert(assignedOperator && "Infix operator node must match one operator.");
    (void)assignedOperator;

    auto &rightMember = node->*Right;
    rightMember = right;
    rightMember->setContainer(*node);

    return node;
  }
};

template <typename Model, typename... Operators> struct InfixRuleOperatorCatalog {
  template <std::size_t... Is>
  static auto make_ops_impl(std::index_sequence<Is...>, Operators &&...in) {
    auto tup = std::tuple<Operators...>{move_if_owned<Operators>(in)...};
    ((std::get<Is>(tup).setPrecedence(
         static_cast<std::int32_t>(sizeof...(Operators) - Is))),
     ...);
    return tup;
  }

  static auto make_ops(Operators &&...in) {
    return make_ops_impl(std::index_sequence_for<Operators...>{},
                         move_if_owned<Operators>(in)...);
  }

  template <std::size_t... Is>
  static const grammar::InfixOperator *
  get_op_impl(const Model *self, std::size_t elementIndex,
              std::index_sequence<Is...>) noexcept {
    using AccessorFn = const grammar::InfixOperator *(*)(const Model *) noexcept;

    static constexpr std::array<AccessorFn, sizeof...(Operators)> accessors = {
        +[](const Model *self) noexcept -> const grammar::InfixOperator * {
          return std::addressof(std::get<Is>(self->ops));
        }...};

    return accessors[elementIndex](self);
  }

  static const grammar::InfixOperator *
  getOperator(const Model *self, std::size_t index) noexcept {
    if (index >= sizeof...(Operators)) {
      return nullptr;
    }
    return get_op_impl(self, index,
                       std::make_index_sequence<sizeof...(Operators)>{});
  }

  static constexpr std::size_t operatorCount() noexcept {
    return sizeof...(Operators);
  }
};

template <typename OperatorExpr>
[[nodiscard]] static constexpr std::int32_t
infix_next_min_precedence(std::int32_t precedence) noexcept {
  if constexpr (std::remove_cvref_t<OperatorExpr>::kAssociativity ==
                grammar::InfixOperator::Associativity::Left) {
    return precedence + 1;
  } else {
    return precedence;
  }
}

template <typename Model, typename... Operators> struct InfixRuleExpectSupport {
  template <std::size_t I = 0>
  static bool try_match_operator(const Model *model, ExpectContext &ctx,
                                 std::int32_t minPrecedence,
                                 std::int32_t &nextMinPrecedence) {
    if constexpr (I == sizeof...(Operators)) {
      return false;
    } else {
      const auto &op = std::get<I>(model->ops);
      constexpr auto precedence =
          static_cast<std::int32_t>(sizeof...(Operators) - I);
      if (precedence < minPrecedence) {
        return try_match_operator<I + 1>(model, ctx, minPrecedence,
                                         nextMinPrecedence);
      }

      const auto checkpoint = ctx.mark();
      auto noEditGuard = ctx.withEditTrackingDisabled();
      (void)noEditGuard;
      if (parse(op, ctx)) {
        if (ctx.frontierBlocked()) {
          ctx.rewind(checkpoint);
        } else {
          nextMinPrecedence =
              infix_next_min_precedence<decltype(op)>(precedence);
          return true;
        }
      }
      ctx.rewind(checkpoint);
      return try_match_operator<I + 1>(model, ctx, minPrecedence,
                                       nextMinPrecedence);
    }
  }

  template <std::size_t I = 0>
  static bool try_merge_operator_frontier(const Model *model, ExpectContext &ctx,
                                          std::int32_t minPrecedence) {
    if constexpr (I == sizeof...(Operators)) {
      return false;
    } else {
      const auto &op = std::get<I>(model->ops);
      constexpr auto precedence =
          static_cast<std::int32_t>(sizeof...(Operators) - I);
      bool merged = false;
      if (precedence >= minPrecedence) {
        const auto checkpoint = ctx.mark();
        auto noEditGuard = ctx.withEditTrackingDisabled();
        (void)noEditGuard;
        if (parse(op, ctx) && ctx.frontierBlocked()) {
          auto operatorFrontier = capture_frontier_since(ctx, checkpoint);
          ctx.rewind(checkpoint);
          if (!operatorFrontier.items.empty()) {
            merge_captured_frontier(ctx, operatorFrontier, false);
          }
          merged = true;
        } else {
          ctx.rewind(checkpoint);
        }
      }
      const bool tailMerged =
          try_merge_operator_frontier<I + 1>(model, ctx, minPrecedence);
      return merged || tailMerged;
    }
  }
};

} // namespace pegium::parser::detail
