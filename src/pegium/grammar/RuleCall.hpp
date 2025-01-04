#pragma once

#include <pegium/grammar/IContext.hpp>
#include <pegium/grammar/IGrammarElement.hpp>
#include <pegium/grammar/IRule.hpp>
#include <pegium/syntax-tree.hpp>
#include <string_view>

namespace pegium::grammar {

template <typename Rule>
  requires IsRule<Rule>
struct RuleCall : IGrammarElement {
  //using type = typename Rule::type;
  constexpr ~RuleCall() override = default;
  explicit constexpr RuleCall(const Rule &rule) : _rule{rule} {}
  constexpr std::size_t parse_rule(std::string_view sv, CstNode &node,
                                   IContext &c) const override {
    return _rule.parse_rule(sv, node, c);
  }

  constexpr std::size_t
  parse_terminal(std::string_view sv) const noexcept override {
    return _rule.parse_terminal(sv);
  }
  void print(std::ostream &os) const override {
    os << _rule;
  }
  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::RuleCall;
  }
private:
  const Rule &_rule;
};

} // namespace pegium::grammar