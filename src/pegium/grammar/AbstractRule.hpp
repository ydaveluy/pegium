#pragma once
#include <cassert>
#include <pegium/grammar/Group.hpp>
#include <pegium/grammar/IGrammarElement.hpp>
#include <pegium/grammar/IRule.hpp>
#include <string_view>

namespace pegium::grammar {

struct AbstractRule : IRule {

  template <typename Element>
    requires(IsGrammarElement<Element>)
  constexpr AbstractRule(std::string_view name, Element &&element)
      : _name{name} {

    *this = element;
  }

  AbstractRule(const AbstractRule &) = delete;
  AbstractRule &operator=(const AbstractRule &) = delete;

  AbstractRule(AbstractRule &&) = delete;
  AbstractRule &operator=(AbstractRule &&) = delete;

  template <typename Element>
    requires IsRule<Element>
  AbstractRule &operator=(Element &&element) {
    this->element = _elements
                        .emplace_back(std::make_unique<Group<Element>>(
                            std::forward_as_tuple<Element>(element)))
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
    this->element =
        _elements
            .emplace_back(
                std::make_unique<GrammarElementType<Element>>(element))
            .get();
    return *this;
  }

  MatchResult parse_terminal(std::string_view sv) const noexcept final {
    assert(element && "The rule definition is missing !");
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
  const std::string &getName() const noexcept { return _name; }

private:
  std::vector<std::unique_ptr<IGrammarElement>> _elements;
  std::string _name;
};
} // namespace pegium::grammar