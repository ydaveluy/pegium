#pragma once
#include <memory>
#include <pegium/grammar/AndPredicate.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/ParseContext.hpp>

namespace pegium::parser {

template <ParseExpression Element>
struct AndPredicate final : grammar::AndPredicate {
  static constexpr bool nullable = true;

  explicit constexpr AndPredicate(Element &&element)
      : _element{std::forward<Element>(element)} {}
  constexpr AndPredicate(AndPredicate &&) noexcept = default;
  constexpr AndPredicate(const AndPredicate &) = default;
  constexpr AndPredicate &operator=(AndPredicate &&) noexcept = default;
  constexpr AndPredicate &operator=(const AndPredicate &) = default;

  const AbstractElement *getElement() const noexcept override {
    return std::addressof(_element);
  }

  bool rule(ParseContext &ctx) const {
    if (ctx.isStrictNoEditMode()) {
      return rule_strict_probe(ctx);
    }

    const bool allowInsert = ctx.allowInsert;
    const bool allowDelete = ctx.allowDelete;
    ctx.allowInsert = false;
    ctx.allowDelete = false;
    const bool result = rule_strict_probe(ctx);
    ctx.allowInsert = allowInsert;
    ctx.allowDelete = allowDelete;
    return result;
  }
  constexpr MatchResult terminal(const char *begin,
                                       const char *end) const noexcept {
    return _element.terminal(begin, end).IsValid()
               ? MatchResult::success(begin)
               : MatchResult::failure(begin);
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

template <ParseExpression Element> constexpr auto operator&(Element &&element) {
  return AndPredicate<Element>{std::forward<Element>(element)};
}
} // namespace pegium::parser
