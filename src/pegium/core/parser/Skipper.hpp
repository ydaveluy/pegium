#pragma once

/// Generic token skipper wrapper used by parser contexts.

#include <concepts>
#include <memory>
#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/syntax-tree/CstBuilder.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace pegium::parser {

template <typename ContextT>
concept ParseContextType =
    requires(const ContextT &ctx, const char *begin, CstBuilder &builder) {
  {
    ctx.skip(begin, builder)
  } noexcept -> std::same_as<const char *>;
};

using SkipHiddenNodesFn =
    const char *(*)(const void *, const char *, CstBuilder &) noexcept;
using SkipWithoutBuilderFn = const char *(*)(const void *, const char *) noexcept;

template <typename ContextT>
  requires ParseContextType<ContextT>
[[nodiscard]] const char *skip_hidden_nodes_for_context(
    const void *contextPtr, const char *begin, CstBuilder &builder) noexcept {
  return static_cast<const ContextT *>(contextPtr)->skip(begin, builder);
}

template <typename ContextT>
  requires requires(const ContextT &ctx, const char *begin) {
    { ctx.skip(begin) } noexcept -> std::same_as<const char *>;
  }
[[nodiscard]] const char *
skip_without_builder_for_context(const void *contextPtr,
                                 const char *begin) noexcept {
  return static_cast<const ContextT *>(contextPtr)->skip(begin);
}

struct Skipper {
  const void *context = nullptr;
  SkipHiddenNodesFn skipFn = &default_skip;
  SkipWithoutBuilderFn skipWithoutBuilderFn = &default_skip_without_builder;
  std::shared_ptr<const void> owner;

  [[nodiscard]] static const char *default_skip(const void *, const char *begin,
                                                CstBuilder &) noexcept {
    return begin;
  }
  [[nodiscard]] static const char *
  default_skip_without_builder(const void *, const char *begin) noexcept {
    return begin;
  }

  template <typename ContextT>
    requires ParseContextType<ContextT>
  [[nodiscard]] static Skipper from(const ContextT &contextInstance) noexcept {
    return {&contextInstance,
            &skip_hidden_nodes_for_context<ContextT>,
            &skip_without_builder_for_context<ContextT>,
            {}};
  }

  template <typename ContextT>
    requires ParseContextType<std::remove_cvref_t<ContextT>>
  [[nodiscard]] static Skipper owning(ContextT &&contextInstance) {
    using StoredContext = std::remove_cvref_t<ContextT>;
    auto ownedContext = std::make_shared<StoredContext>(
        std::forward<ContextT>(contextInstance));
    return {ownedContext.get(),
            &skip_hidden_nodes_for_context<StoredContext>,
            &skip_without_builder_for_context<StoredContext>,
            std::move(ownedContext)};
  }

  [[nodiscard]] const char *
  skip(const char *begin, CstBuilder &builder) const noexcept {
    return skipFn(context, begin, builder);
  }

  [[nodiscard]] const char *
  skip(const std::string &text, CstBuilder &builder) const noexcept {
    return skip(text.c_str(), builder);
  }

  [[nodiscard]] const char *skip(const char *begin) const noexcept {
    return skipWithoutBuilderFn(context, begin);
  }

  [[nodiscard]] const char *skip(const std::string &text) const noexcept {
    return skip(text.c_str());
  }

};

[[nodiscard]] inline const Skipper &NoOpSkipper() noexcept {
  static const Skipper skipper{};
  return skipper;
}

} // namespace pegium::parser
