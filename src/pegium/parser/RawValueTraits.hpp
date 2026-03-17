#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include <pegium/grammar/ParserRule.hpp>
#include <pegium/parser/OrderedChoice.hpp>
#include <pegium/parser/RuleValue.hpp>
#include <pegium/parser/ValueBuildContext.hpp>

namespace pegium::parser::detail {

template <typename> struct AcceptAnyRawValue : std::true_type {};

template <typename T> struct RawValueDependentFalse : std::false_type {};

template <typename Expr>
concept HasRawValueMethod = requires(const std::remove_cvref_t<Expr> &expression,
                                     const CstNodeView &node) {
  expression.getRawValue(node);
};

template <typename Expr>
using RawValueType =
    std::remove_cvref_t<decltype(std::declval<const std::remove_cvref_t<Expr> &>()
                                     .getRawValue(std::declval<const CstNodeView &>()))>;

template <typename Expr, template <typename> class RawValuePredicate,
          bool = HasRawValueMethod<Expr>>
struct HasSpecificRawValue : std::false_type {};

template <typename Expr, template <typename> class RawValuePredicate>
struct HasSpecificRawValue<Expr, RawValuePredicate, true>
    : std::bool_constant<!std::same_as<RawValueType<Expr>, grammar::RuleValue> &&
                         !IsStdVariant<RawValueType<Expr>>::value &&
                         RawValuePredicate<RawValueType<Expr>>::value> {};

template <typename Expr, template <typename> class RawValuePredicate>
struct ExpressionRawCompliantImpl
    : HasSpecificRawValue<Expr, RawValuePredicate> {};

template <Expression... Choices, template <typename> class RawValuePredicate>
struct ExpressionRawCompliantImpl<OrderedChoice<Choices...>, RawValuePredicate>
    : std::bool_constant<
          (ExpressionRawCompliantImpl<Choices, RawValuePredicate>::value && ...)> {
};

template <Expression... Choices, template <typename> class RawValuePredicate>
struct ExpressionRawCompliantImpl<OrderedChoiceWithSkipper<Choices...>,
                                  RawValuePredicate>
    : std::bool_constant<
          (ExpressionRawCompliantImpl<Choices, RawValuePredicate>::value && ...)> {
};

template <typename Expr,
          template <typename> class RawValuePredicate = AcceptAnyRawValue>
struct ExpressionRawCompliant
    : ExpressionRawCompliantImpl<std::remove_cvref_t<Expr>, RawValuePredicate> {
};

template <typename Expr,
          template <typename> class RawValuePredicate = AcceptAnyRawValue>
inline constexpr bool expression_raw_compliant_v =
    ExpressionRawCompliant<Expr, RawValuePredicate>::value;

template <typename Expr>
[[gnu::always_inline]] inline decltype(auto)
extract_raw_value(const Expr &expression, const CstNodeView &node,
                  const ValueBuildContext &context) {
  if constexpr (requires { expression.getRawValue(node, context); }) {
    return expression.getRawValue(node, context);
  } else if constexpr (requires { expression.getRawValue(node); }) {
    return expression.getRawValue(node);
  } else {
    static_assert(RawValueDependentFalse<Expr>::value,
                  "Expression must expose getRawValue(node) or "
                  "getRawValue(node, context).");
  }
}

template <std::size_t I = 0, typename OrderedChoiceExpr, typename Visitor>
[[gnu::always_inline]] inline bool visit_selected_ordered_choice_raw_value(
    const OrderedChoiceExpr &orderedChoice,
    const grammar::AbstractElement *selectedGrammarElement,
    const CstNodeView &selectedNode, const ValueBuildContext &context,
    Visitor &&visitor) {
  if constexpr (I ==
                std::tuple_size_v<
                    std::remove_cvref_t<decltype(orderedChoice.choices)>>) {
    return false;
  } else {
    const auto &choice = std::get<I>(orderedChoice.choices);
    if (std::addressof(choice) == selectedGrammarElement) {
      visitor(extract_raw_value(choice, selectedNode, context));
      return true;
    }
    return visit_selected_ordered_choice_raw_value<I + 1>(
        orderedChoice, selectedGrammarElement, selectedNode, context, std::forward<Visitor>(visitor));
  }
}

} // namespace pegium::parser::detail
