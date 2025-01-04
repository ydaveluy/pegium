#pragma once

#include <pegium/grammar/IAssignment.hpp>
#include <pegium/grammar/IRule.hpp>
#include <source_location>

namespace pegium::grammar {

template <typename Element, typename AttrType>
concept IsValidRule = IsGrammarElement<Element> &&
                          requires(Element &rule, const pegium::CstNode &node) {
                            {
                              rule.getValue(node)
                            } -> std::convertible_to<AttrType>;
                          } ||
                      std::same_as<bool, AttrType>;

template <auto feature, typename Element>
// requires IsValidRule<Element, AttrType>
struct Assignment final : IAssignment {

  constexpr explicit Assignment(Element &&element)
      : _element{std::forward<Element>(element)} {}
  constexpr std::size_t parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const override {

    auto index = parent.content.size();
    auto i = _element.parse_rule(sv, parent, c);
    if (success(i)) {
      // override the grammar source
      parent.content[index].grammarSource = this;
    }
    return i;
  }
  constexpr std::size_t
  parse_terminal(std::string_view) const noexcept override {
    assert(false && "An Assignment cannot be in a terminal.");
    return PARSE_ERROR;
  }

  void execute(AstNode *current, const CstNode &node) const override {
    do_execute(current, feature, node);
  }

  void print(std::ostream &os) const override {
    os << member_name<feature>() << "=" << _element;
  }

  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::Assignment;
  }

private:
  Element _element;
  template <typename ClassType, typename AttrType>
  void do_execute(AstNode *current, AttrType ClassType::*member,
                  const CstNode &node) const {
    auto *value = dynamic_cast<ClassType *>(current);
    if(!value)

    assert(value && "Tryed to assign a feature on an AstNode with wrong type.");

    if constexpr (std::is_same_v<bool, AttrType> &&
                  !std::is_same_v<
                      bool, typename std::remove_cvref_t<Element>::type>) {

      value->*feature = true;
    } else if constexpr (std::is_base_of_v<IRule, std::remove_cvref_t<Element>>){

        helpers::AssignmentHelper<AttrType>{}(value->*feature, _element.getValue(node));
     // value->*feature = _element.getValue(node);
    }else
    {
        helpers::AssignmentHelper<AttrType>{}(value->*feature, std::string(node.text));
    }
    // TODO set _container on new value if AstNode + index if vector + feature
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
template <auto e, typename Element>
// requires IsValidRule<Element, e>
  requires std::is_member_pointer_v<decltype(e)>
static constexpr auto assign(Element &&element) {
  return Assignment<e, GrammarElementType<Element>>(
      std::forward<Element>(element));
}

/*template <typename T, typename Element>
// requires IsValidRule<Element, e>
// requires std::is_member_pointer_v<decltype(e)>
static constexpr auto assign(Element &&element) {
  return Assignment<nullptr, GrammarElementType<Element>>(
      std::forward<Element>(element));
}*/

} // namespace pegium::grammar