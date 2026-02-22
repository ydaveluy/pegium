#pragma once

#include <pegium/grammar/NotPredicate.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/ParseState.hpp>
#include <pegium/parser/RecoverState.hpp>
namespace pegium::parser {

template <ParseExpression Element>
struct NotPredicate final : grammar::NotPredicate {
  explicit constexpr NotPredicate(Element &&element)
      : _element{std::forward<Element>(element)} {}
  constexpr NotPredicate(NotPredicate &&) noexcept = default;
  constexpr NotPredicate(const NotPredicate &) = default;
  constexpr NotPredicate &operator=(NotPredicate &&) noexcept = default;
  constexpr NotPredicate &operator=(const NotPredicate &) = default;
  
  const AbstractElement *getElement() const noexcept override {
    return std::addressof(_element);
  }

  constexpr bool parse_rule(ParseState &s) const {
    const auto mark = s.mark();
    const bool result = _element.parse_rule(s);
    s.rewind(mark);
    return !result;
  }
  bool recover(RecoverState &recoverState) const {
    if (recoverState.isStrictNoEditMode()) {
      return !recover_strict_probe(recoverState);
    }

    const bool allowInsert = recoverState.allowInsert;
    const bool allowDelete = recoverState.allowDelete;
    recoverState.allowInsert = false;
    recoverState.allowDelete = false;
    const bool result = recover_strict_probe(recoverState);
    recoverState.allowInsert = allowInsert;
    recoverState.allowDelete = allowDelete;
    return !result;
  }

  constexpr MatchResult parse_terminal(const char *begin,
                                       const char *end) const noexcept {
    return _element.parse_terminal(begin, end).IsValid()
               ? MatchResult::failure(begin)
               : MatchResult::success(begin);
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_terminal(sv.begin(), sv.end());
  }

private:
  ParseExpressionHolder<Element> _element;

  bool recover_strict_probe(RecoverState &recoverState) const {
    const bool trackEditState = recoverState.trackEditState;
    recoverState.setTrackEditState(false);
    const auto mark = recoverState.mark();
    const bool result = _element.recover(recoverState);
    recoverState.rewind(mark);
    recoverState.setTrackEditState(trackEditState);
    return result;
  }
};

template <ParseExpression Element>
constexpr auto operator!(Element &&element) {
  return NotPredicate<Element>{std::forward<Element>(element)};
}

} // namespace pegium::parser
