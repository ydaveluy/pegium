#pragma once

/// Generic wrapper that attaches a local Skipper override to a parser
/// combinator (Group, OrderedChoice, Repetition, UnorderedGroup) without
/// duplicating the inheritance + parse_impl + init_impl boilerplate at
/// every site.

#include <memory>
#include <utility>

#include <pegium/core/parser/AstReflectionBootstrap.hpp>
#include <pegium/core/parser/CompletionSupport.hpp>
#include <pegium/core/parser/Introspection.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/Skipper.hpp>

namespace pegium::parser {

template <typename Wrapped>
struct SkipperWrapped final : Wrapped, CompletionSkipperProvider {
  using Base = Wrapped;

  explicit SkipperWrapped(const Base &base, Skipper localSkipper)
      : Base(base), _localSkipper(std::move(localSkipper)) {}
  explicit SkipperWrapped(Base &&base, Skipper localSkipper)
      : Base(std::move(base)), _localSkipper(std::move(localSkipper)) {}

  SkipperWrapped(SkipperWrapped &&) noexcept = default;
  SkipperWrapped(const SkipperWrapped &) = default;
  SkipperWrapped &operator=(SkipperWrapped &&) noexcept = default;
  SkipperWrapped &operator=(const SkipperWrapped &) = default;

  [[nodiscard]] const Skipper *getCompletionSkipper() const noexcept override {
    return std::addressof(_localSkipper);
  }

private:
  friend struct detail::ParseAccess;
  friend struct detail::InitAccess;

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    auto guard = ctx.with_skipper(_localSkipper);
    (void)guard;
    return parse(static_cast<const Base &>(*this), ctx);
  }

  void init_impl(AstReflectionInitContext &ctx) const {
    static_cast<const Base &>(*this).init_impl(ctx);
  }

  Skipper _localSkipper;
};

} // namespace pegium::parser
