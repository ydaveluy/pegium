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

struct AbstractWrapper {
  virtual ~AbstractWrapper() noexcept = default;
  MatchResult parse_terminal(const char *begin,
                             const char *end) const noexcept {
    assert(_parse_terminal);
    return _parse_terminal(this, begin, end);
  }
  MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_terminal(sv.begin(), sv.end());
  }

  bool parse_rule(ParseState &s) const {
    assert(_parse_rule);
    return _parse_rule(this, s);
  }

  bool recover(RecoverState &recover) const {
    assert(_recover);
    return _recover(this, recover);
  }

  const grammar::AbstractElement *getElement() const noexcept {
    assert(_get_element);
    return _get_element(this);
  }

protected:
  using ParseTerminalFn =
      MatchResult (*)(const AbstractWrapper *, const char *,
                      const char *) noexcept;
  using ParseRuleFn = bool (*)(const AbstractWrapper *, ParseState &);
  using RecoverFn = bool (*)(const AbstractWrapper *, RecoverState &);
  using GetElementFn =
      const grammar::AbstractElement *(*)(const AbstractWrapper *) noexcept;

  ParseTerminalFn _parse_terminal = nullptr;
  ParseRuleFn _parse_rule = nullptr;
  RecoverFn _recover = nullptr;
  GetElementFn _get_element = nullptr;
};
template <ParseExpression Element> struct Wrapper final : AbstractWrapper {
  Wrapper(Element &&element) : holder{std::forward<Element>(element)} {
    this->_parse_terminal = [](const AbstractWrapper *self, const char *begin,
                               const char *end) noexcept -> MatchResult {
      return static_cast<const Wrapper *>(self)->holder.parse_terminal(begin,
                                                                       end);
    };
    this->_parse_rule = [](const AbstractWrapper *self,
                           ParseState &s) -> bool {
      return static_cast<const Wrapper *>(self)->holder.parse_rule(s);
    };
    this->_recover = [](const AbstractWrapper *self,
                        RecoverState &recover) -> bool {
      return static_cast<const Wrapper *>(self)->holder.recover(recover);
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
struct AbstractRuleBase : GrammarRule {

  template <ParseExpression Element>
  constexpr AbstractRuleBase(std::string_view name, Element &&element)
      : _name{name} {

    *this = std::forward<Element>(element);
  }
  const grammar::AbstractElement *getElement() const noexcept override {
    assert(_assignedElement && "The rule definition is missing !");
    return _assignedElement;
  }

  AbstractRuleBase(const AbstractRuleBase &) = delete;
  AbstractRuleBase &operator=(const AbstractRuleBase &) = delete;

  AbstractRuleBase(AbstractRuleBase &&) = delete;
  AbstractRuleBase &operator=(AbstractRuleBase &&) = delete;

  /// Initialize the rule with an element
  /// @tparam Element
  /// @param element the element
  /// @return a reference to the rule
  template <ParseExpression Element>
  AbstractRuleBase &operator=(Element &&element) {
    auto wrapper = std::make_unique<Wrapper<Element>>(
        std::forward<Element>(element));
    auto *holder = std::addressof(wrapper->holder);
    _assignedObject = holder;
    _assignedParseTerminal = &parse_assigned_terminal_impl<Element>;
    _assignedParseRule = &parse_assigned_rule_impl<Element>;
    _assignedParseRecover = &parse_assigned_recover_impl<Element>;
    _assignedElement = static_cast<const grammar::AbstractElement *>(holder);

    this->element = wrapper.get();
    _elements.emplace_back(std::move(wrapper));
    return *this;
  }

  MatchResult parse_terminal(const char *begin, const char *end) const noexcept {
    return parse_assigned_terminal_fast(begin, end);
  }
  MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_terminal(sv.begin(), sv.end());
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
  bool parse_assigned_rule(ParseState &s) const {
    assert(_assignedObject && _assignedParseRule &&
           "The rule definition is missing !");
    return _assignedParseRule(_assignedObject, s);
  }

  bool parse_assigned_recover(RecoverState &recover) const {
    assert(_assignedObject && _assignedParseRecover &&
           "The rule definition is missing !");
    return _assignedParseRecover(_assignedObject, recover);
  }

  MatchResult parse_assigned_terminal_fast(const char *begin,
                                           const char *end) const noexcept {
    assert(_assignedObject && _assignedParseTerminal &&
           "The rule definition is missing !");
    return _assignedParseTerminal(_assignedObject, begin, end);
  }

private:
  using AssignedParseTerminalFn =
      MatchResult (*)(const void *, const char *, const char *) noexcept;
  using AssignedParseRuleFn = bool (*)(const void *, ParseState &);
  using AssignedParseRecoverFn = bool (*)(const void *, RecoverState &);

  template <ParseExpression Element>
  static MatchResult parse_assigned_terminal_impl(const void *object,
                                                  const char *begin,
                                                  const char *end) noexcept {
    using HolderType = std::remove_reference_t<ParseExpressionHolder<Element>>;
    return static_cast<const HolderType *>(object)->parse_terminal(begin, end);
  }

  template <ParseExpression Element>
  static bool parse_assigned_rule_impl(const void *object, ParseState &s) {
    using HolderType = std::remove_reference_t<ParseExpressionHolder<Element>>;
    return static_cast<const HolderType *>(object)->parse_rule(s);
  }

  template <ParseExpression Element>
  static bool parse_assigned_recover_impl(const void *object,
                                          RecoverState &recover) {
    using HolderType = std::remove_reference_t<ParseExpressionHolder<Element>>;
    return static_cast<const HolderType *>(object)->recover(recover);
  }

  std::vector<std::unique_ptr<AbstractWrapper>> _elements;
  std::string _name;
  const void *_assignedObject = nullptr;
  AssignedParseTerminalFn _assignedParseTerminal = nullptr;
  AssignedParseRuleFn _assignedParseRule = nullptr;
  AssignedParseRecoverFn _assignedParseRecover = nullptr;
  const grammar::AbstractElement *_assignedElement = nullptr;
};

struct AbstractRule : AbstractRuleBase<grammar::DataTypeRule> {
  using BaseRule = AbstractRuleBase<grammar::DataTypeRule>;
  using BaseRule::BaseRule;
  using BaseRule::operator=;
};
} // namespace pegium::parser
