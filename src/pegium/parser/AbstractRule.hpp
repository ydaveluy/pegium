#pragma once
#include <cassert>
#include <memory>
#include <pegium/grammar/Rule.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace pegium::parser {

struct AbstractWrapper : pegium::grammar::AbstractElement{

  static constexpr bool nullable = false;
  ~AbstractWrapper() noexcept override = default;
  MatchResult terminal(const char *begin,
                             const char *end) const noexcept {
    assert(_terminal);
    return _terminal(this, begin, end);
  }
  MatchResult terminal(std::string_view sv) const noexcept {
    return terminal(sv.begin(), sv.end());
  }

  bool rule(ParseContext &ctx) const {
    assert(_rule);
    return _rule(this, ctx);
  }

  const grammar::AbstractElement *getElement() const noexcept {
    assert(_get_element);
    return _get_element(this);
  }
  constexpr  ElementKind getKind() const noexcept override {
    assert(_get_element);
    return _get_element(this)->getKind();
  }
  constexpr void print(std::ostream &os) const override {
    assert(_get_element);
    _get_element(this)->print(os);
  }


protected:
  using TerminalFn = MatchResult (*)(const AbstractWrapper *, const char *,
                                          const char *) noexcept;
  using RuleFn = bool (*)(const AbstractWrapper *, ParseContext &);
  using GetElementFn =
      const grammar::AbstractElement *(*)(const AbstractWrapper *) noexcept;

  TerminalFn _terminal = nullptr;
  RuleFn _rule = nullptr;
  GetElementFn _get_element = nullptr;
};
template <ParseExpression Element> struct Wrapper final : AbstractWrapper {

  Wrapper(Element &&element) : holder{std::forward<Element>(element)} {
    this->_terminal = [](const AbstractWrapper *self, const char *begin,
                               const char *end) noexcept -> MatchResult {
      return static_cast<const Wrapper *>(self)->holder.terminal(begin,
                                                                       end);
    };

    this->_rule = [](const AbstractWrapper *self,
                        ParseContext &ctx) -> bool {
      return static_cast<const Wrapper *>(self)->holder.rule(ctx);
    };
    this->_get_element = [](const AbstractWrapper *self) noexcept
        -> const grammar::AbstractElement * {
      return std::addressof(static_cast<const Wrapper *>(self)->holder);
    };
  }

  ParseExpressionHolder<Element> holder;
};

template <typename GrammarRule>
  requires std::derived_from<GrammarRule, grammar::AbstractRule>
struct AbstractRule : GrammarRule {
  static constexpr bool nullable = false;

  template <ParseExpression Element>
  constexpr AbstractRule(std::string_view name, Element &&element)
      : _name{name} {
    *this = std::forward<Element>(element);
  }
  const grammar::AbstractElement *getElement() const noexcept override {
    assert(_assignedElement && "The rule definition is missing !");
    return _assignedElement;
  }

  AbstractRule(const AbstractRule &) = delete;
  AbstractRule &operator=(const AbstractRule &) = delete;

  AbstractRule(AbstractRule &&) = delete;
  AbstractRule &operator=(AbstractRule &&) = delete;

  /// Initialize the rule with an element
  /// @tparam Element
  /// @param element the element
  /// @return a reference to the rule
  template <ParseExpression Element>
  AbstractRule &operator=(Element &&element) {
        static_assert(!std::remove_cvref_t<Element>::nullable,
                  "A rule cannot be initialized with a nullable element.");
    auto wrapper =
        std::make_unique<Wrapper<Element>>(std::forward<Element>(element));
    auto *holder = std::addressof(wrapper->holder);
    _assignedObject = holder;
    _assignedTerminal = &terminal_impl<Element>;
    _assignedRule = &rule_impl<Element>;
    _assignedElement = static_cast<const grammar::AbstractElement *>(holder);

    this->element = wrapper.get();
    _elements.emplace_back(std::move(wrapper));
    return *this;
  }

  MatchResult terminal(const char *begin,
                             const char *end) const noexcept {
    return terminal_fast(begin, end);
  }
  MatchResult terminal(std::string_view sv) const noexcept {
    return terminal(sv.begin(), sv.end());
  }
  /** get the super implementation of the rule */
   AbstractWrapper &super() const {
    assert(element &&
           "cannot call super on a rule if it is not already defined.");
    return *element;
  }
  void print(std::ostream &os) const override { os << _name; }

protected:
  AbstractWrapper *element = nullptr;
  const std::string &getName() const noexcept { return _name; }

  bool rule_fast(ParseContext &ctx) const {
    assert(_assignedObject && _assignedRule &&
           "The rule definition is missing !");
    return _assignedRule(_assignedObject, ctx);
  }

  MatchResult terminal_fast(const char *begin,
                                           const char *end) const noexcept {
    assert(_assignedObject && _assignedTerminal &&
           "The rule definition is missing !");
    return _assignedTerminal(_assignedObject, begin, end);
  }

private:
  using AssignedTerminalFn = MatchResult (*)(const void *, const char *,
                                                  const char *) noexcept;
  using AssignedRuleFn = bool (*)(const void *, ParseContext &);

  template <ParseExpression Element>
  static MatchResult terminal_impl(const void *object,
                                                  const char *begin,
                                                  const char *end) noexcept {
    using HolderType = std::remove_reference_t<ParseExpressionHolder<Element>>;
    return static_cast<const HolderType *>(object)->terminal(begin, end);
  }

  template <ParseExpression Element>
  static bool rule_impl(const void *object,
                                          ParseContext &rule) {
    using HolderType = std::remove_reference_t<ParseExpressionHolder<Element>>;
    return static_cast<const HolderType *>(object)->rule(rule);
  }

  std::vector<std::unique_ptr<AbstractWrapper>> _elements;
  std::string _name;
  const void *_assignedObject = nullptr;
  AssignedTerminalFn _assignedTerminal = nullptr;
  AssignedRuleFn _assignedRule = nullptr;
  const grammar::AbstractElement *_assignedElement = nullptr;
};


} // namespace pegium::parser
