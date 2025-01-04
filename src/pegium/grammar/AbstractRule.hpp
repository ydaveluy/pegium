#pragma once
#include <cassert>
#include <pegium/grammar/IGrammarElement.hpp>
#include <pegium/grammar/IRule.hpp>
#include <pegium/grammar/RuleCall.hpp>
#include <string_view>

namespace pegium::grammar {

struct AbstractRule : IRule {

  AbstractRule(std::string_view name,
               std::string_view description = "")
      : _name{name}, _description{description} {}
  AbstractRule(const AbstractRule &) = delete;
  AbstractRule &operator=(const AbstractRule &) = delete;

  template <typename Element>
    requires IsRule<Element>
  AbstractRule &operator=(Element &&element) {
    this->element = _elements
                   .emplace_back(std::make_unique<RuleCall<Element>>(
                       std::forward<Element>(element)))
                   .get();
    return *this;
  }
  /// Initialize the rule with an element
  /// @tparam Element
  /// @param element the element
  /// @return a reference to the rule
  template <typename Element>
    requires(IsGrammarElement<Element> && !IsRule<Element>)
  AbstractRule &operator=(Element &&element) {
    this->element = _elements
                   .emplace_back(std::make_unique<GrammarElementType<Element>>(
                       std::forward<Element>(element)))
                   .get();
    return *this;
  }

  std::size_t parse_terminal(std::string_view sv) const noexcept final {
    assert(element&&"The rule definition is missing !");
    return element->parse_terminal(sv);
  }
  /** get the super implementation of the rule */
  inline IGrammarElement &super() const {
    assert(element &&
           "cannot call super on a rule if it is not already defined.");
    return *element;
  }
  void print(std::ostream &os) const override { os << "RULE(" << _name << ")"; }

protected:
  IGrammarElement *element = nullptr;

private:
  std::vector<std::unique_ptr<IGrammarElement>> _elements;
  std::string _name;
  std::string _description;
};
} // namespace pegium::grammar