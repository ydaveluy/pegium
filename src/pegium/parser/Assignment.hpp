#pragma once

#include <cassert>
#include <pegium/grammar/Assignment.hpp>
#include <pegium/grammar/Rule.hpp>
// #include <pegium/parser/IRule.hpp>
#include <pegium/parser/AbstractElement.hpp>
#include <pegium/parser/OrderedChoice.hpp>
#include <source_location>
#include <type_traits>

namespace pegium::parser {

template <auto feature, ParserExpression Element>
struct IsValidAssignment
    : std::bool_constant<(
          (
              // If the Element type is an AstNode
              std::derived_from<helpers::AttrType<feature>, AstNode> &&
              (
                  // Check that the element type is convertible to AttrType
                  std::derived_from<typename std::remove_cvref_t<Element>::type,
                                    helpers::AttrType<feature>>)) ||
          // If the element Type is not an AstType
          (
              // Check that the type is convertible to AttrType
              std::convertible_to<typename std::remove_cvref_t<Element>::type,
                                  helpers::AttrType<feature>> ||
              // or AttrType constructible from the givent type
              std::constructible_from<
                  helpers::AttrType<feature>,
                  typename std::remove_cvref_t<Element>::type> ||
              // or the feature is callable with the givent type
              /*std::is_invocable_v<
                  decltype(feature), helpers::ClassType<feature>,
                  typename std::remove_cvref_t<Element>::type> ||*/
              // Or the AttrType is a boolean
              std::same_as<bool, helpers::AttrType<feature>>))> {};

template <auto feature, typename... Element>
struct IsValidAssignment<feature, OrderedChoice<Element...>>
    : std::bool_constant<(IsValidAssignment<feature, Element>::value && ...)> {
};

template <auto feature, typename Element>
// requires IsValidRule<Element, AttrType>
struct Assignment final : grammar::Assignment {

  constexpr explicit Assignment(Element &&element)
      : _element{std::forward<Element>(element)} {}

  constexpr Assignment(Assignment &&) = default;
  constexpr Assignment(const Assignment &) = default;
  constexpr Assignment &operator=(Assignment &&) = default;
  constexpr Assignment &operator=(const Assignment &) = default;

  const grammar::AbstractElement *getElement() const noexcept override {
    return std::addressof(_element);
  }
  std::string_view getFeature() const noexcept override {
    return member_name<feature>();
  }
  constexpr AssignmentOperator getOperator() const noexcept override {
    // TODO return the correct operator depending on feature type
    return AssignmentOperator::Append;
  }

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const {

    if constexpr (IsOrderedChoice<Element>::value) {
      CstNode node;
      auto i = _element.parse_rule(sv, node, c);
      if (i) {
        node.grammarSource = this;
        parent.content.emplace_back(std::move(node));
      }
      return i;
    } else {
      // if the element is not an ordered choice we can save one CstNode by
      // overriding the grammar source of the first inserted sub node
      auto index = parent.content.size();
      auto i = _element.parse_rule(sv, parent, c);
      if (i) {
        // override the grammar source
        parent.content[index].grammarSource = this;
      }
      return i;
    }
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    assert(false && "An Assignment cannot be in a terminal.");
    return MatchResult::failure(sv.begin());
  }

  void execute(AstNode *current, const CstNode &node) const override {
    do_execute(current, feature, node);
  }

private:
  ParserExpressionHolder<Element> _element;
  template <typename ClassType, typename AttrType>
  void do_execute(AstNode *current, AttrType ClassType::*,
                  const CstNode &node) const {
    assert(dynamic_cast<ClassType *>(current) &&
           "Tryed to assign a feature on an AstNode with wrong type.");

    auto *astNode = static_cast<ClassType *>(current);
    // TODO protect with try catch and store error in context
    if constexpr (IsOrderedChoice<Element>::value) {
      if constexpr (std::is_base_of_v<AstNode, helpers::AttrType<feature>>) {
        assert(
            dynamic_cast<const grammar::Rule *>(node.content[0].grammarSource));
        const auto *rule =
            static_cast<const grammar::Rule *>(node.content[0].grammarSource);
        auto any = rule->getAnyValue(node);
        auto ptr = std::any_cast<std::shared_ptr<AstNode>>(any);
        assert(std::dynamic_pointer_cast<helpers::AttrType<feature>>(ptr));
        helpers::AssignmentHelper<AttrType>{}(
            astNode, feature,
            std::static_pointer_cast<helpers::AttrType<feature>>(ptr));
      }
    } else if constexpr (std::is_same_v<bool, AttrType> &&
                         !std::is_same_v<bool, typename std::remove_cvref_t<
                                                   Element>::type>) {
      helpers::AssignmentHelper<AttrType>{}(astNode, feature, true);
    } else if constexpr (std::is_base_of_v<AstNode,
                                           helpers::AttrType<feature>>) {

      helpers::AssignmentHelper<AttrType>{}(astNode, feature,
                                            _element.getValue(node));
    } else if constexpr (std::is_base_of_v<grammar::Rule,
                                           std::remove_cvref_t<Element>>) {

      helpers::AssignmentHelper<AttrType>{}(astNode, feature,
                                            _element.getValue(node));
    } else {
      helpers::AssignmentHelper<AttrType>{}(astNode, feature,
                                            std::string(node.text));
    }
  }
  template <auto... Vs>
  [[nodiscard]] static constexpr auto function_name() noexcept
      -> std::string_view {
    return std::source_location::current().function_name();
  }
  template <class... Ts>
  [[nodiscard]] static constexpr auto function_name() noexcept
      -> std::string_view {
    return std::source_location::current().function_name();
  }

  /// Helper to get the name of a member from a member object pointer
  /// @tparam e the member object pointer
  /// @return the name of the member
  template <auto e> static constexpr std::string_view member_name() noexcept {
    std::string_view func_name = function_name<e>();
    func_name = func_name.substr(0, func_name.rfind(REF_STRUCT::end_marker));
    return func_name.substr(func_name.rfind("::") + 2);
  }
  struct REF_STRUCT {
    int MEMBER;

    static constexpr auto name = function_name<&REF_STRUCT::MEMBER>();
    static constexpr auto end_marker =
        name.substr(name.find("REF_STRUCT::MEMBER") +
                    std::string_view{"REF_STRUCT::MEMBER"}.size());
  };
};

/// Assign an element to a member of the current object
/// @tparam Element
/// @tparam e the member pointer
/// @param args the list of grammar elements
/// @return
template <auto feature, typename Element>
  requires IsValidAssignment<feature, Element>::value
static constexpr auto assign(Element &&element) {
  return Assignment<feature, Element>(std::forward<Element>(element));
}

} // namespace pegium::parser