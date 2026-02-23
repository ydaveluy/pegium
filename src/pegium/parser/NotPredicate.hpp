#pragma once

#include <pegium/grammar/NotPredicate.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/ParseContext.hpp>

namespace pegium::parser {

template <ParseExpression Element>
struct NotPredicate final : grammar::NotPredicate {
  static constexpr bool nullable = true;
  explicit constexpr NotPredicate(Element &&element)
      : _element{std::forward<Element>(element)} {}
  constexpr NotPredicate(NotPredicate &&) noexcept = default;
  constexpr NotPredicate(const NotPredicate &) = default;
  constexpr NotPredicate &operator=(NotPredicate &&) noexcept = default;
  constexpr NotPredicate &operator=(const NotPredicate &) = default;
  
  const AbstractElement *getElement() const noexcept override {
    return std::addressof(_element);
  }

  bool rule(ParseContext &ctx) const {
    if (ctx.isStrictNoEditMode()) {
      return !rule_strict_probe(ctx);
    }

    const bool allowInsert = ctx.allowInsert;
    const bool allowDelete = ctx.allowDelete;
    ctx.allowInsert = false;
    ctx.allowDelete = false;
    const bool result = rule_strict_probe(ctx);
    ctx.allowInsert = allowInsert;
    ctx.allowDelete = allowDelete;
    return !result;
  }

  constexpr MatchResult terminal(const char *begin,
                                       const char *end) const noexcept {
    return _element.terminal(begin, end).IsValid()
               ? MatchResult::failure(begin)
               : MatchResult::success(begin);
  }
  constexpr MatchResult terminal(std::string_view sv) const noexcept {
    return terminal(sv.begin(), sv.end());
  }

private:
  ParseExpressionHolder<Element> _element;

  bool rule_strict_probe(ParseContext &ctx) const {
    const bool trackEditState = ctx.trackEditState;
    ctx.setTrackEditState(false);
    const auto mark = ctx.mark();
    const bool result = _element.rule(ctx);
    ctx.rewind(mark);
    ctx.setTrackEditState(trackEditState);
    return result;
  }
};

template <ParseExpression Element>
constexpr auto operator!(Element &&element) {
  return NotPredicate<Element>{std::forward<Element>(element)};
}

} // namespace pegium::parser
