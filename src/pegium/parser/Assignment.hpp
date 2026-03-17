#pragma once

#include <pegium/grammar/Assignment.hpp>
#include <pegium/parser/AssignmentSupport.hpp>
#include <pegium/parser/Introspection.hpp>
#include <pegium/parser/OrderedChoice.hpp>
#include <pegium/parser/ParseMode.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/RawValueTraits.hpp>
#include <pegium/parser/ValueBuildContext.hpp>
#include <type_traits>
#include <utility>
#include <string_view>

namespace pegium::parser {

template <auto feature, NonNullableExpression Element>
struct Assignment final : grammar::Assignment {
private:
  static constexpr bool assignment_has_required_raw_values =
      detail::expression_raw_compliant_v<Element>;

  static_assert(
      assignment_has_required_raw_values,
      "Assignment requires getRawValue(node): direct expression must provide "
      "a specific typed value (not grammar::RuleValue); for OrderedChoice, "
      "each choice must provide getRawValue(node) with a specific typed value.");

public:
  using ReferenceSupport = detail::AssignmentReferenceSupport<feature>;

  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe =
      !IsOrderedChoice<Element>::value &&
      std::remove_cvref_t<Element>::isFailureSafe;

  explicit constexpr Assignment(Element &&element, AssignmentOperator ope)
    requires(!std::is_lvalue_reference_v<Element>)
      : _element(std::move(element)), _operator{ope} {}
  explicit constexpr Assignment(Element element, AssignmentOperator ope)
    requires(std::is_lvalue_reference_v<Element>)
      : _element(element), _operator{ope} {}

  constexpr Assignment(Assignment &&) noexcept = default;
  constexpr Assignment(const Assignment &) = default;
  constexpr Assignment &operator=(Assignment &&) noexcept = default;
  constexpr Assignment &operator=(const Assignment &) = default;

  const grammar::AbstractElement *getElement() const noexcept override {
    return std::addressof(_element);
  }
  std::string_view getFeature() const noexcept override {
    return detail::member_name_v<feature>;
  }
  constexpr AssignmentOperator getOperator() const noexcept override {
    return _operator;
  }
  [[nodiscard]] bool isReference() const noexcept override {
    return ReferenceSupport::isReference;
  }
  [[nodiscard]] bool isMultiReference() const noexcept override {
    return ReferenceSupport::isMultiReference;
  }
  [[nodiscard]] std::type_index getReferenceType() const noexcept override {
    return ReferenceSupport::getReferenceType();
  }
  [[nodiscard]] bool
  acceptsReferenceTarget(const AstNode *node) const noexcept override {
    return ReferenceSupport::acceptsReferenceTarget(node);
  }
  constexpr bool isNullable() const noexcept override {
    return nullable;
  }
  bool probeRecoverable(RecoveryContext &ctx) const {
    return attempt_fast_probe(ctx, _element) ||
           probe_locally_recoverable(_element, ctx);
  }

  void execute(AstNode *current, const CstNodeView &node,
               const ValueBuildContext &context) const override {
    detail::AssignmentRuntimeSupport<feature, Element>::execute(
        _element, current, node, context.withProperty(getFeature()));
  }
  grammar::FeatureValue getValue(const AstNode *ast) const override {
    return detail::AssignmentRuntimeSupport<feature, Element>::getValue(ast);
  }

private:
  friend struct detail::ParseAccess;

  ExpressionHolder<Element> _element;
  AssignmentOperator _operator;

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    return detail::AssignmentParseSupport<feature, Element>::parse(ctx, this,
                                                                   _element);
  }
};

/// Assign an element to a member of the current object
/// @tparam feature the member pointer
/// @tparam Element the parse expression
/// @param element the expression
/// @return the assignment
template <auto feature, NonNullableExpression Element>
  requires(helpers::IsValidAssignment<feature, Element>::value &&
           !helpers::IsMany<feature>)
static constexpr auto assign(Element &&element) {
  return Assignment<feature, Element>(
      std::forward<Element>(element),
      pegium::grammar::AssignmentOperator::Assign);
}

/// Append an element to a member of the current object
/// @tparam feature the member pointer
/// @tparam Element the parse expression
/// @param element the expression
/// @return the assignment
template <auto feature, NonNullableExpression Element>
  requires helpers::IsValidAssignment<feature, Element>::value &&
           helpers::IsMany<feature>
static constexpr auto append(Element &&element) {
  return Assignment<feature, Element>(
      std::forward<Element>(element),
      pegium::grammar::AssignmentOperator::Append);
}

/// Enable a member of the current object
/// @tparam feature the boolean member pointer
/// @tparam Element the parse expression
/// @param element the expression
/// @return the assignment
template <auto feature, NonNullableExpression Element>
  requires std::same_as<bool, helpers::AttrType<feature>>
static constexpr auto enable_if(Element &&element) {
  return Assignment<feature, Element>(
      std::forward<Element>(element),
      pegium::grammar::AssignmentOperator::EnableIf);
}
} // namespace pegium::parser
