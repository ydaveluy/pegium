

#pragma once
#include <cassert>
#include <map>
#include <pegium/grammar/IContext.hpp>
#include <pegium/grammar/TerminalRule.hpp>
#include <pegium/syntax-tree.hpp>
#include <string_view>
#include <tuple>

namespace pegium::grammar {

template <typename Tuple, typename Func>
void for_each(Tuple &&tuple, Func &&func) {
  std::apply([&](auto &...elems) { (func(elems), ...); }, tuple);
}

template <typename HiddenTuple, typename IgnoredTuple> struct Context;
template <typename... Hidden, typename... Ignored>
/*requires(std::derived_from<std::remove_cvref_t<Hidden>, TerminalRule<>> &&
         ...) &&
        (std::derived_from<std::remove_cvref_t<Ignored>, TerminalRule<>> &&
         ...)*/
struct Context<std::tuple<Hidden &...>, std::tuple<Ignored &...>> final
    : IContext {

  template <typename... H, typename... I>
  Context(std::tuple<H &...> &&hiddens, std::tuple<I &...> &&ignored)
      : _hidden{std::forward<std::tuple<H &...>>(hiddens)},
        _ignored{std::forward<std::tuple<I &...>>(ignored)} {}

  MatchResult skipHiddenNodes(std::string_view sv,
                              CstNode &node) const override {

    MatchResult i = MatchResult::success(sv.begin());

    do {

      for_each(_ignored, [&](const auto &rule) {
        i |= rule.parse_terminal({i.offset, sv.end()});
      });
      i.valid = false;

      for_each(_hidden, [&](const auto &rule) {
        const auto len = rule.parse_terminal({i.offset, sv.end()});
        if (len) {
          auto &hiddenNode = node.content.emplace_back();
          hiddenNode.text = {i.offset, len.offset};
          hiddenNode.grammarSource = &rule;
          hiddenNode.hidden = true;

          i |= len;
        }
      });

    } while (i);
    i.valid = true;

    return i;
  }

  void addRecovery(std::string_view expected,
                   std::string_view position) override {
    recoveries.emplace_back(RecoveryEntry{std::string(expected), position});
  }
  virtual void clearRecovery() override { recoveries.clear(); }

  void setInputText(std::string_view text) override { _text = text; }

private:
  std::string_view _text;
  struct RecoveryEntry {
    std::string expected;
    std::string_view position;
  };
  std::vector<RecoveryEntry> recoveries;
  std::tuple<Hidden &...> _hidden;
  std::tuple<Ignored &...> _ignored;
};
template <typename HiddenTuple = std::tuple<>,
          typename IgnoredTuple = std::tuple<>>
struct ContextBuilder {
  ContextBuilder(HiddenTuple &&hiddens = std::tie(),
                 IgnoredTuple &&ignored = std::tie())
      : _hidden{std::move(hiddens)}, _ignored{std::move(ignored)} {}

  template <typename... Ignored>
  /*requires (std::derived_from<std::remove_cvref_t<Ignored>, TerminalRule<>> &&
           ...) && (std::tuple_size<IgnoredTuple>::value == 0)*/
  ContextBuilder<HiddenTuple, std::tuple<Ignored &...>>
  ignore(Ignored &&...ignored) {
    return ContextBuilder<HiddenTuple, std::tuple<Ignored &...>>{
        std::move(_hidden),
        std::tuple<Ignored &...>(std::forward<Ignored>(ignored)...)};
  }

  template <typename... Hidden>
  /*requires(std::derived_from<std::remove_cvref_t<Hidden>, TerminalRule<>> &&
           ...) && (std::tuple_size<HiddenTuple>::value == 0)*/
  ContextBuilder<std::tuple<Hidden &...>, IgnoredTuple>
  hide(Hidden &&...hidden) {
    return ContextBuilder<std::tuple<Hidden &...>, IgnoredTuple>{
        std::tuple<Hidden &...>(std::forward<Hidden>(hidden)...),
        std::move(_ignored)};
  }

  auto build() {
    return std::make_unique<Context<HiddenTuple, IgnoredTuple>>(
        std::move(_hidden), std::move(_ignored));
  }

private:
  HiddenTuple _hidden;
  IgnoredTuple _ignored;
};
ContextBuilder() -> ContextBuilder<>;

template <typename... H, typename... I>
Context(std::tuple<H &...> &&, std::tuple<I &...> &&)
    -> Context<std::tuple<H &...>, std::tuple<I &...>>;

} // namespace pegium::grammar