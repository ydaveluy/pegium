#pragma once
#include <cassert>
#include <pegium/grammar/Rule.hpp>
#include <pegium/parser/AbstractElement.hpp>
#include <pegium/parser/Group.hpp>
#include <string_view>

namespace pegium::parser {

struct AbstractWrapper {
  virtual ~AbstractWrapper() noexcept = default;
  virtual MatchResult parse_terminal(std::string_view sv) const noexcept = 0;

  virtual MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                 IContext &c) const = 0;

  virtual const grammar::AbstractElement *getElement() const noexcept = 0;
};
template <ParserExpression Element> struct Wrapper final : AbstractWrapper {
  Wrapper(Element &&element) : holder{std::forward<Element>(element)} {}
  MatchResult parse_terminal(std::string_view sv) const noexcept override {
    return holder.parse_terminal(sv);
  }
  MatchResult parse_rule(std::string_view sv, CstNode &parent,
                         IContext &c) const override {
    return holder.parse_rule(sv, parent, c);
  }
  const grammar::AbstractElement *getElement() const noexcept override {
    return std::addressof(holder);
  }

private:
  ParserExpressionHolder<Element> holder;
};

struct AbstractRule : grammar::Rule {

  template <ParserExpression Element>
  constexpr AbstractRule(std::string_view name, Element &&element)
      : _name{name} {

    *this = std::forward<Element>(element);
  }
  const AbstractElement *getElement() const noexcept override {
    return element->getElement();
  }

  AbstractRule(const AbstractRule &) = delete;
  AbstractRule &operator=(const AbstractRule &) = delete;

  AbstractRule(AbstractRule &&) = delete;
  AbstractRule &operator=(AbstractRule &&) = delete;

  /// Initialize the rule with an element
  /// @tparam Element
  /// @param element the element
  /// @return a reference to the rule
  template <ParserExpression Element>
  // requires(!grammar::IsRule<Element>)
  AbstractRule &operator=(Element &&element) {
    this->element = _elements
                        .emplace_back(std::make_unique<Wrapper<Element>>(
                            std::forward<Element>(element)))
                        .get();
    return *this;
  }

  MatchResult parse_terminal(std::string_view sv) const noexcept {
    assert(element && "The rule definition is missing !");
    return element->parse_terminal(sv);
  }
  /** get the super implementation of the rule */
  inline AbstractWrapper &super() const {
    assert(element &&
           "cannot call super on a rule if it is not already defined.");
    return *element;
  }
  void print(std::ostream &os) const override { os << _name; }

protected:
  AbstractWrapper *element = nullptr;
  const std::string &getName() const noexcept { return _name; }

private:
  std::vector<std::unique_ptr<AbstractWrapper>> _elements;
  std::string _name;
};
} // namespace pegium::parser