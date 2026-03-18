#pragma once

#include <array>
#include <concepts>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <pegium/grammar/InfixRule.hpp>
#include <pegium/parser/ExpectContext.hpp>
#include <pegium/parser/ExpectFrontier.hpp>
#include <pegium/parser/Introspection.hpp>
#include <pegium/parser/ParseAttempt.hpp>
#include <pegium/parser/ParseMode.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/RawValueTraits.hpp>
#include <pegium/parser/RuleValue.hpp>
#include <pegium/parser/ValueBuildContext.hpp>
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
  using LeftPointeeType = typename LeftType::element_type;
  using RightPointeeType = typename RightType::element_type;

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
  template <typename RawValue>
  static grammar::RuleValue to_rule_value(RawValue &&rawValue) {
    using RawType = std::remove_cvref_t<RawValue>;
    if constexpr (detail::SupportedRuleValueType<RawType>) {
      return detail::toRuleValue(std::forward<RawValue>(rawValue));
    } else {
      static_assert(detail::RawValueDependentFalse<RawType>::value,
                    "InfixOperator raw value must be convertible to grammar::RuleValue.");
    }
  }

  template <typename Visitor>
  static bool visit_raw_value(const Element &element, const CstNodeView &node,
                              const ValueBuildContext &context,
                              Visitor &&visitor) {
    auto &&visitorRef = visitor;
    if constexpr (IsOrderedChoice<Element>::value) {
      for (const auto view : node) {
        const auto &child = view.node();
        if (child.isHidden) {
          continue;
        }

        const auto *grammarElement = child.grammarElement;
        assert(grammarElement && "Node must have a grammar element");
        return detail::visit_selected_ordered_choice_raw_value(
            element, grammarElement, view, context, visitorRef);
      }
      return false;
    } else {
      visitorRef(detail::extract_raw_value(element, node, context));
      return true;
    }
  }

  static grammar::RuleValue get_value(const Element &element,
                                      const CstNodeView &node) {
    ValueBuildContext context;
    grammar::RuleValue value{std::int8_t{0}};
    const bool visited =
        visit_raw_value(element, node, context, [&value](auto &&rawValue) {
          value = to_rule_value(std::forward<decltype(rawValue)>(rawValue));
        });
    if (!visited) {
      return grammar::RuleValue{std::int8_t{0}};
    }
    return value;
  }
};

template <typename Model, typename NodeType, auto Left, auto Op, auto Right,
          typename OpType, typename LeftPointeeType, typename RightPointeeType,
          typename... Operators>
struct InfixRuleValueBuilder {
  static std::unique_ptr<AstNode>
  getValue(const Model *model, const CstNodeView &node,
           std::unique_ptr<AstNode> lhsNode, const ValueBuildContext &context) {
    return cast_operand<AstNode>(buildTypedFromCst(
        model, node, cast_operand<LeftPointeeType>(std::move(lhsNode)), context));
  }

private:
  template <typename Target, typename Source>
  static std::unique_ptr<Target> cast_operand(std::unique_ptr<Source> value) {
    static_assert(std::derived_from<Source, AstNode>,
                  "InfixRule operands must derive from AstNode.");
    static_assert(std::derived_from<Target, AstNode>,
                  "InfixRule target operands must derive from AstNode.");
    assert(value != nullptr);
    auto *astNode = static_cast<AstNode *>(value.release());
    return std::unique_ptr<Target>(static_cast<Target *>(astNode));
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
      const auto &operatorExpression = std::get<I>(model->ops);
      if (operatorNode.getGrammarElement() == std::addressof(operatorExpression)) {
        return assign_operator_from_expression(node, operatorExpression,
                                               operatorNode, context);
      }
      return assign_operator_from_node<I + 1>(model, node, operatorNode,
                                              context);
    }
  }

  static std::unique_ptr<NodeType>
  buildTypedFromCst(const Model *model, const CstNodeView &node,
                    std::unique_ptr<LeftPointeeType> lhsNode,
                    const ValueBuildContext &context) {
    std::unique_ptr<RightPointeeType> rhs;
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
          buildTypedFromCst(model, child,
                            cast_operand<LeftPointeeType>(std::move(rhs)),
                            context));
    }

    assert(lhsNode != nullptr);
    assert(rhs != nullptr);
    assert(operatorNode.valid());
    return createNode(model, node, std::move(lhsNode), operatorNode,
                      std::move(rhs), context);
  }

  static std::unique_ptr<NodeType>
  createNode(const Model *model, const CstNodeView &cstNode,
             std::unique_ptr<LeftPointeeType> left,
             const CstNodeView &operatorNode,
             std::unique_ptr<RightPointeeType> right,
             const ValueBuildContext &context) {
    auto node = std::make_unique<NodeType>();
    node->setCstNode(cstNode);

    auto &leftMember = node.get()->*Left;
    leftMember = std::move(left);
    leftMember->attachToContainer(*node, detail::member_name_v<Left>);

    const bool assignedOperator =
        assign_operator_from_node(model, node.get(), operatorNode, context);
    assert(assignedOperator && "Infix operator node must match one operator.");
    (void)assignedOperator;

    auto &rightMember = node.get()->*Right;
    rightMember = std::move(right);
    rightMember->attachToContainer(*node, detail::member_name_v<Right>);

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
      auto noEditGuard = ctx.withEditState(false, false, false);
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
        auto noEditGuard = ctx.withEditState(false, false, false);
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
