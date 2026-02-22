#pragma once

#include <cassert>
#include <exception>
#include <memory>
#include <optional>
#include <pegium/grammar/AnyCharacter.hpp>
#include <pegium/grammar/Assignment.hpp>
#include <pegium/grammar/CharacterRange.hpp>
#include <pegium/grammar/Literal.hpp>
#include <pegium/grammar/Rule.hpp>
// #include <pegium/parser/IRule.hpp>
#include <pegium/parser/AssignmentHelpers.hpp>
#include <pegium/parser/Introspection.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/ParseState.hpp>
#include <pegium/parser/RecoverState.hpp>
#include <pegium/parser/OrderedChoice.hpp>
#include <pegium/parser/RuleValue.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace pegium::parser {

template <auto feature, ParseExpression Element>
struct IsValidAssignment
    : std::bool_constant<
          (
              // If the Element type is an AstNode
              std::derived_from<helpers::AttrType<feature>, AstNode> &&

              // Check that the element type is convertible to AttrType
              std::derived_from<typename std::remove_cvref_t<Element>::type,
                                helpers::AttrType<feature>>) ||
          // If the element Type is not an AstType
          (
              // Check that the type is convertible to AttrType
              std::convertible_to<typename std::remove_cvref_t<Element>::type,
                                  helpers::AttrType<feature>> ||
              // or AttrType constructible from the given type
              std::constructible_from<
                  helpers::AttrType<feature>,
                  typename std::remove_cvref_t<Element>::type> ||
              // or AttrType constructible from a shared_ptr of the given type
              std::constructible_from<
                  helpers::AttrType<feature>,
                  std::shared_ptr<typename std::remove_cvref_t<Element>::type>>

              )> {};

template <auto feature, typename... Element>
struct IsValidAssignment<feature, OrderedChoice<Element...>>
    : std::bool_constant<(IsValidAssignment<feature, Element>::value && ...)> {
};

template <auto feature, typename... Element>
struct IsValidAssignment<feature, const OrderedChoice<Element...>>
    : IsValidAssignment<feature, OrderedChoice<Element...>> {};

template <auto feature, typename... Element>
struct IsValidAssignment<feature, OrderedChoice<Element...> &>
    : IsValidAssignment<feature, OrderedChoice<Element...>> {};

template <auto feature, typename... Element>
struct IsValidAssignment<feature, const OrderedChoice<Element...> &>
    : IsValidAssignment<feature, OrderedChoice<Element...>> {};

template <auto feature, typename... Element>
struct IsValidAssignment<feature, OrderedChoice<Element...> &&>
    : IsValidAssignment<feature, OrderedChoice<Element...>> {};

template <auto feature, typename... Element>
struct IsValidAssignment<feature, const OrderedChoice<Element...> &&>
    : IsValidAssignment<feature, OrderedChoice<Element...>> {};

template <auto feature, ParseExpression Element>
struct Assignment final : grammar::Assignment {

  constexpr Assignment(Element &&element, AssignmentOperator ope)
      : _element{std::forward<Element>(element)}, _operator{ope} {}

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

  constexpr bool parse_rule(ParseState &s) const {
    if constexpr (IsOrderedChoice<Element>::value) {
      const auto mark = s.enter();
      if (!_element.parse_rule(s)) {
        s.rewind(mark);
        return false;
      }
      s.exit(this);
      return true;
    } else {
      const auto before = s.node_count();
      const bool result = _element.parse_rule(s);
      if (result && s.node_count() > before) {
        s.override_grammar_element(static_cast<NodeId>(before), this);
      }
      return result;
    }
  }
  bool recover(RecoverState &recoverState) const {
    if (recoverState.isStrictNoEditMode()) {
      return recover_strict_impl(recoverState);
    }
    return recover_editable_impl(recoverState);
  }
  constexpr MatchResult parse_terminal(const char *begin,
                                       const char *) const noexcept {
    assert(false && "An Assignment cannot be in a terminal.");
    return MatchResult::failure(begin);
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_terminal(sv.begin(), sv.end());
  }

  void execute(AstNode *current, const CstNodeView &node) const override {
    do_execute(current, feature, node);
  }

private:
  ParseExpressionHolder<Element> _element;
  AssignmentOperator _operator;

  bool recover_strict_impl(RecoverState &recoverState) const {
    if constexpr (IsOrderedChoice<Element>::value) {
      const auto mark = recoverState.enter();
      if (!_element.recover(recoverState)) {
        recoverState.rewind(mark);
        return false;
      }
      recoverState.exit(this);
      return true;
    } else {
      const auto before = recoverState.node_count();
      const bool result = _element.recover(recoverState);
      if (result && recoverState.node_count() > before) {
        recoverState.override_grammar_element(static_cast<NodeId>(before), this);
      }
      return result;
    }
  }

  bool recover_editable_impl(RecoverState &recoverState) const {
    if constexpr (IsOrderedChoice<Element>::value) {
      const auto mark = recoverState.enter();
      if (!_element.recover(recoverState)) {
        recoverState.rewind(mark);
        return false;
      }
      recoverState.exit(this);
      return true;
    } else {
      const auto before = recoverState.node_count();
      const bool result = _element.recover(recoverState);
      if (result && recoverState.node_count() > before) {
        recoverState.override_grammar_element(static_cast<NodeId>(before), this);
      }
      return result;
    }
  }

  template <typename T> struct IsVariant : std::false_type {};
  template <typename... Ts>
  struct IsVariant<std::variant<Ts...>> : std::true_type {};
  template <typename E, typename = void> struct ElementValueType {
    using type = void;
  };
  template <typename E>
  struct ElementValueType<E,
                          std::void_t<typename std::remove_cvref_t<E>::type>> {
    using type = typename std::remove_cvref_t<E>::type;
  };
  template <typename T> struct IsSharedPtr : std::false_type {};
  template <typename T>
  struct IsSharedPtr<std::shared_ptr<T>> : std::true_type {
    using type = T;
  };
  template <typename T> struct IsVectorSharedPtr : std::false_type {};
  template <typename T>
  struct IsVectorSharedPtr<std::vector<std::shared_ptr<T>>> : std::true_type {
    using type = T;
  };
  template <typename T> struct IsOptional : std::false_type {};
  template <typename T> struct IsOptional<std::optional<T>> : std::true_type {
    using type = T;
  };

  using SourceValue =
      std::variant<grammar::RuleValue, std::shared_ptr<AstNode>>;

  struct SelectedValue {
    SourceValue value;
    ElementKind kind;
  };

  template <typename ClassType, typename AttrType, typename Value>
  bool assign_direct_strict(ClassType *astNode, AttrType ClassType::*member,
                            Value &&value) const {
    using TargetValueType = helpers::AttrType<feature>;
    using RawValueType = std::remove_cvref_t<Value>;

    // a Reference accept only string and string_view
    if constexpr (pegium::is_reference_v<AttrType>) {
      if constexpr (std::same_as<RawValueType, std::string>) {
        helpers::AssignmentHelper<AttrType>{}(astNode, member, value);
        return true;
      } else if constexpr (std::same_as<RawValueType, std::string_view>) {
        helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                              std::string{value});
        return true;
      }
      return false;
    }

    if constexpr (IsOptional<AttrType>::value) {
      using OptionalValueType = typename IsOptional<AttrType>::type;
      if constexpr (pegium::is_reference_v<OptionalValueType>) {
        if constexpr (std::same_as<RawValueType, std::string>) {
          helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                                OptionalValueType{value});
          return true;
        } else if constexpr (std::same_as<RawValueType, std::string_view>) {
          helpers::AssignmentHelper<AttrType>{}(
              astNode, member, OptionalValueType{std::string{value}});
          return true;
        }
        return false;
      }

      if constexpr (std::same_as<RawValueType, OptionalValueType>) {
        helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                              std::forward<Value>(value));
        return true;
      } else if constexpr (std::same_as<OptionalValueType, std::string> &&
                           std::same_as<RawValueType, std::string_view>) {
        helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                              std::string{value});
        return true;
      }
      return false;
    }

    if constexpr (IsSharedPtr<AttrType>::value ||
                  IsVectorSharedPtr<AttrType>::value) {
      using PointeeType =
          typename std::conditional_t<IsSharedPtr<AttrType>::value,
                                      IsSharedPtr<AttrType>,
                                      IsVectorSharedPtr<AttrType>>::type;

      if constexpr (IsSharedPtr<RawValueType>::value) {
        using ValuePointeeType = typename IsSharedPtr<RawValueType>::type;
        if constexpr (std::derived_from<ValuePointeeType, PointeeType>) {
          auto ptr = std::remove_cvref_t<Value>{std::forward<Value>(value)};
          helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                                std::move(ptr));
          return true;
        } else if constexpr (std::derived_from<ValuePointeeType, AstNode> &&
                             std::derived_from<PointeeType, AstNode>) {
          // Safe by design: IsValidAssignment ensures compile-time compatibility
          // between assigned rule values and target attribute types.
          // Keep a debug-time RTTI check as a guard against malformed CST wiring.
          assert((!value || std::dynamic_pointer_cast<PointeeType>(value)));
          if (auto casted = std::static_pointer_cast<PointeeType>(value);
              casted) {
            helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                                  std::move(casted));
            return true;
          }
        }
      } else if constexpr (std::derived_from<RawValueType, PointeeType>) {
        helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                              std::forward<Value>(value));
        return true;
      }
      return false;
    }

    if constexpr (std::same_as<RawValueType, TargetValueType>) {
      helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                            std::forward<Value>(value));
      return true;
    } else if constexpr (std::same_as<TargetValueType, std::string> &&
                         std::same_as<RawValueType, std::string_view>) {
      helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                            std::string{value});
      return true;
    } else if constexpr (std::same_as<TargetValueType, std::nullptr_t> &&
                         std::same_as<RawValueType, std::nullptr_t>) {
      helpers::AssignmentHelper<AttrType>{}(astNode, member, nullptr);
      return true;
    }

    return false;
  }

  template <typename VariantType, typename Value, std::size_t I = 0>
  bool try_make_variant_exact(VariantType &target, const Value &value) const {
    if constexpr (I == std::variant_size_v<VariantType>) {
      return false;
    } else {
      using Alternative = std::variant_alternative_t<I, VariantType>;
      if constexpr (std::same_as<Alternative, std::remove_cvref_t<Value>>) {
        target = value;
        return true;
      }
      return try_make_variant_exact<VariantType, Value, I + 1>(target, value);
    }
  }

  template <typename VariantType, std::size_t I = 0>
  bool
  try_make_variant_from_ast_ptr(VariantType &target,
                                const std::shared_ptr<AstNode> &value) const {
    if constexpr (I == std::variant_size_v<VariantType>) {
      return false;
    } else {
      using Alternative = std::variant_alternative_t<I, VariantType>;
      if constexpr (IsSharedPtr<Alternative>::value) {
        using AlternativePointee = typename IsSharedPtr<Alternative>::type;
        if constexpr (std::derived_from<AlternativePointee, AstNode>) {
          if (auto casted = std::dynamic_pointer_cast<AlternativePointee>(value);
              casted) {
            target = std::move(casted);
            return true;
          }
        }
      }
      return try_make_variant_from_ast_ptr<VariantType, I + 1>(target, value);
    }
  }

  template <typename VariantType>
  bool try_make_variant_from_rule_value(VariantType &target,
                                        const grammar::RuleValue &value) const {
    bool assigned = false;
    std::visit(
        [&]<typename T>(const T &item) {
          if (assigned) {
            return;
          }
          assigned = try_make_variant_exact(target, item);
          if constexpr (std::same_as<std::remove_cvref_t<decltype(item)>,
                                     std::string_view>) {
            if (!assigned) {
              assigned = try_make_variant_exact(target, std::string{item});
            }
          }
        },
        value);
    return assigned;
  }

  template <typename ClassType, typename AttrType>
  bool try_assign_rule_value(ClassType *astNode, AttrType ClassType::*member,
                             const grammar::RuleValue &value) const {
    using TargetValueType = helpers::AttrType<feature>;
    if constexpr (IsVariant<TargetValueType>::value) {
      TargetValueType target;
      if (!try_make_variant_from_rule_value(target, value)) {
        return false;
      }
      return assign_direct_strict(astNode, member, std::move(target));
    }

    bool assigned = false;
    std::visit(
        [&]<typename T>(const T &item) {
          if (assigned) {
            return;
          }
          assigned = assign_direct_strict(astNode, member, item);
          if constexpr (std::same_as<std::remove_cvref_t<decltype(item)>,
                                     std::string_view>) {
            if (!assigned) {
              assigned =
                  assign_direct_strict(astNode, member, std::string{item});
            }
          }
        },
        value);
    return assigned;
  }

  template <typename ClassType, typename AttrType>
  bool try_assign_ast_ptr(ClassType *astNode, AttrType ClassType::*member,
                          const std::shared_ptr<AstNode> &value) const {
    using TargetValueType = helpers::AttrType<feature>;
    if constexpr (IsVariant<TargetValueType>::value) {
      TargetValueType target;
      if (!try_make_variant_from_ast_ptr(target, value)) {
        return false;
      }
      return assign_direct_strict(astNode, member, std::move(target));
    }

    return assign_direct_strict(astNode, member, value);
  }

  template <typename ClassType, typename AttrType>
  bool try_assign_source_value(ClassType *astNode, AttrType ClassType::*member,
                               const SourceValue &value) const {
    return std::visit(
        [&]<typename T>(const T &item) {
          using ValueType = std::remove_cvref_t<decltype(item)>;
          if constexpr (std::same_as<ValueType, grammar::RuleValue>) {
            return try_assign_rule_value(astNode, member, item);
          } else {
            return try_assign_ast_ptr(astNode, member, item);
          }
        },
        value);
  }

  template <typename ClassType, typename AttrType, typename Value>
  bool try_assign_value(ClassType *astNode, AttrType ClassType::*member,
                        Value &&value) const {
    using ValueType = std::remove_cvref_t<Value>;

    if constexpr (detail::IsStdVariant<ValueType>::value) {
      bool assigned = false;
      std::visit(
          [&]<typename T>(const T &item) {
            if (!assigned) {
              assigned = try_assign_value(astNode, member, item);
            }
          },
          value);
      return assigned;
    }

    if constexpr (std::same_as<ValueType, grammar::RuleValue>) {
      return try_assign_rule_value(astNode, member, value);
    }

    if constexpr (IsSharedPtr<ValueType>::value &&
                  std::derived_from<typename IsSharedPtr<ValueType>::type,
                                    AstNode>) {
      return try_assign_ast_ptr(astNode, member,
                                std::static_pointer_cast<AstNode>(value));
    }

    return assign_direct_strict(astNode, member, std::forward<Value>(value));
  }

  std::optional<SelectedValue>
  extract_selected_value(const CstNodeView &node) const {
    for (const auto &child : node) {
      if (child.isHidden()) {
        continue;
      }

      const auto *grammarElement = child.getGrammarElement();
      if (!grammarElement) {
        return std::nullopt;
      }

      switch (grammarElement->getKind()) {
      case ElementKind::TerminalRule:
        return SelectedValue{
            static_cast<const grammar::TerminalRule *>(grammarElement)
                ->getValue(child),
            ElementKind::TerminalRule};
      case ElementKind::DataTypeRule:
        return SelectedValue{
            static_cast<const grammar::DataTypeRule *>(grammarElement)
                ->getValue(child),
            ElementKind::DataTypeRule};
      case ElementKind::ParserRule:
        return SelectedValue{
            static_cast<const grammar::ParserRule *>(grammarElement)
                ->getValue(child),
            ElementKind::ParserRule};
      case ElementKind::Literal:
        return SelectedValue{
            grammar::RuleValue{
                static_cast<const grammar::Literal *>(grammarElement)
                    ->getValue(child)},
            ElementKind::Literal};
      case ElementKind::CharacterRange:
        return SelectedValue{
            grammar::RuleValue{
                static_cast<const grammar::CharacterRange *>(grammarElement)
                    ->getValue(child)},
            ElementKind::CharacterRange};
      case ElementKind::AnyCharacter:
        return SelectedValue{
            grammar::RuleValue{
                static_cast<const grammar::AnyCharacter *>(grammarElement)
                    ->getValue(child)},
            ElementKind::AnyCharacter};
      default:
        return std::nullopt;
      }
    }
    return std::nullopt;
  }

  template <typename ClassType, typename AttrType>
  void do_execute(AstNode *current, AttrType ClassType::*,
                  const CstNodeView &node) const {
    assert(current);
    auto *astNode = static_cast<ClassType *>(current);

    if constexpr (IsOrderedChoice<Element>::value) {
      const auto selected = extract_selected_value(node);
      if (!selected.has_value()) {
        throw std::runtime_error(
            "OrderedChoice has no assignable selected value for " +
            std::string(detail::member_name_v<feature>));
      }
      if (!try_assign_source_value(astNode, feature, selected->value)) {
        throw std::runtime_error("OrderedChoice value is not assignable to " +
                                 std::string(detail::member_name_v<feature>));
      }
    } else if constexpr (std::is_same_v<bool, AttrType> &&
                         !std::is_same_v<
                             bool, typename ElementValueType<Element>::type>) {
      helpers::AssignmentHelper<AttrType>{}(astNode, feature, true);
    } else {
      if constexpr (requires(const std::remove_cvref_t<Element> &element) {
                      element.getValue(node);
                    }) {
        if (try_assign_value(astNode, feature, _element.getValue(node))) {
          return;
        }
      }

      throw std::runtime_error("Assignment value is not assignable to " +
                               std::string(detail::member_name_v<feature>));
    }
  }
};

/// Assign an element to a member of the current object
/// @tparam feature the member pointer
/// @tparam Element the parse expression
/// @param element the expression
/// @return the assignment
template <auto feature, ParseExpression Element>
  requires(IsValidAssignment<feature, Element>::value &&
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
template <auto feature, ParseExpression Element>
  requires IsValidAssignment<feature, Element>::value &&
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
template <auto feature, ParseExpression Element>
  requires std::same_as<bool, helpers::AttrType<feature>>
static constexpr auto enable_if(Element &&element) {
  return Assignment<feature, Element>(
      std::forward<Element>(element),
      pegium::grammar::AssignmentOperator::EnableIf);
}
} // namespace pegium::parser
