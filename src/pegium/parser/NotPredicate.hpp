#pragma once

#include <pegium/grammar/NotPredicate.hpp>
#include <pegium/parser/ExpectContext.hpp>
#include <pegium/parser/ParseAttempt.hpp>
#include <pegium/parser/ParseMode.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <string>
#include <string_view>

namespace pegium::parser {

template <Expression Element>
struct NotPredicate final : grammar::NotPredicate {
  static constexpr bool nullable = true;
  static constexpr bool isFailureSafe = true;
  explicit constexpr NotPredicate(Element &&element)
    requires(!std::is_lvalue_reference_v<Element>)
      : _element(std::move(element)) {}
  explicit constexpr NotPredicate(Element element)
    requires(std::is_lvalue_reference_v<Element>)
      : _element(element) {}

  constexpr NotPredicate(NotPredicate &&) noexcept = default;
  constexpr NotPredicate(const NotPredicate &) = default;
  constexpr NotPredicate &operator=(NotPredicate &&) noexcept = default;
  constexpr NotPredicate &operator=(const NotPredicate &) = default;

  const AbstractElement *getElement() const noexcept override {
    return std::addressof(_element);
  }

  constexpr const char *terminal(const char *begin) const noexcept
    requires TerminalCapableExpression<Element>
  {
    return _element.terminal(begin) != nullptr ? nullptr : begin;
  }
  constexpr const char *terminal(const std::string &text) const noexcept
    requires TerminalCapableExpression<Element>
  {
    return terminal(text.c_str());
  }

  constexpr bool isNullable() const noexcept override {
    return nullable;
  }

private:
  friend struct detail::ParseAccess;
  friend struct detail::ProbeAccess;

  ExpressionHolder<Element> _element;

  bool probe_impl(ParseContext &ctx) const {
    return !parser::probe(_element, ctx);
  }

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    if constexpr (!ExpectParseModeContext<Context>) {
      if constexpr (RecoveryParseModeContext<Context>) {
        TrackedParseContext &strictCtx = ctx;
        return !parser::probe(_element, strictCtx);
      } else {
        ParseContext &strictCtx = ctx;
        return !parser::probe(_element, strictCtx);
      }
    } else {
      if (ctx.cursor() == ctx.end) {
        return !std::remove_cvref_t<Element>::nullable;
      }

      const std::string_view remaining{
          ctx.cursor(), static_cast<std::size_t>(ctx.end - ctx.cursor())};
      auto probeCtx = ExpectContext{remaining, ctx.skipper(),
                                    static_cast<TextOffset>(remaining.size())};
      probeCtx.setInRecoveryPhase(false);
      return with_no_edits(probeCtx,
                           [&]() { return !parser::attempt_fast_probe(probeCtx, _element); });
    }
  }
};

template <Expression Element> constexpr auto operator!(Element &&element) {
  return NotPredicate<Element>{std::forward<Element>(element)};
}

} // namespace pegium::parser
