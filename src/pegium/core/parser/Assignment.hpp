#pragma once

#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/parser/AssignmentSupport.hpp>
#include <pegium/core/parser/Introspection.hpp>
#include <pegium/core/parser/OrderedChoice.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/ParseExpression.hpp>
#include <pegium/core/parser/RawValueTraits.hpp>
#include <pegium/core/parser/AstReflectionBootstrap.hpp>
#include <pegium/core/parser/ValueBuildContext.hpp>
#include <type_traits>
#include <typeindex>
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
  [[nodiscard]] std::type_index getType() const noexcept override {
    const auto &typeInfo = assignmentReflectionInfo();
    if (typeInfo.referenceTargetType != nullptr) {
      return typeInfo.referenceTargetType->type;
    }
    if (typeInfo.assignedAstType != nullptr) {
      return typeInfo.assignedAstType->type;
    }
    return detail::invalid_type();
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
        _element, current, node, context.withAssignment(*this));
  }
  grammar::FeatureValue getValue(const AstNode *ast) const override {
    return detail::AssignmentRuntimeSupport<feature, Element>::getValue(ast);
  }

private:
  friend struct detail::ParseAccess;
  friend struct detail::InitAccess;

  [[nodiscard]] static const detail::AssignmentReflectionInfo &
  assignmentReflectionInfo() noexcept {
    static const detail::AssignmentReflectionInfo info = [] {
      detail::AssignmentReflectionInfo value{};
      using AssignedType = helpers::AttrType<feature>;
      if constexpr (std::derived_from<AssignedType, AstNode>) {
        value.assignedAstType =
            std::addressof(detail::ast_node_type_info<AssignedType>());
      }
      value.referenceTargetType = ReferenceSupport::getReferenceTargetTypeInfo();
      return value;
    }();
    return info;
  }

  ExpressionHolder<Element> _element;
  AssignmentOperator _operator;

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    return detail::AssignmentParseSupport<feature, Element>::parse(ctx, this,
                                                                   _element);
  }

  void init_impl(AstReflectionInitContext &ctx) const {
    const auto &typeInfo = assignmentReflectionInfo();
    ctx.registerAssignment(typeInfo);
    if (typeInfo.assignedAstType != nullptr) {
      auto childContext = ctx.withExpectedType(typeInfo.assignedAstType->type);
      parser::init(_element, childContext);
      return;
    }
    auto childContext = ctx.withoutExpectedType();
    parser::init(_element, childContext);
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
