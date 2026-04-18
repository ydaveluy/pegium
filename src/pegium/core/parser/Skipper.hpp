#pragma once

/// Generic token skipper wrapper used by parser contexts.

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/syntax-tree/CstBuilder.hpp>
#include <span>
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

template <typename ContextT>
concept SkipWithoutBuilderContextType =
    requires(const ContextT &ctx, const char *begin) {
      { ctx.skip(begin) } noexcept -> std::same_as<const char *>;
    };

struct HiddenLeafTraceEntry {
  TextOffset beginOffset = 0;
  TextOffset endOffset = 0;
  const grammar::AbstractElement *element = nullptr;
};

struct SkipTraceResult {
  const char *end = nullptr;
  std::uint32_t hiddenLeafCount = 0;
  bool overflowed = false;
};

template <typename ContextT>
concept SkipTraceContextType =
    requires(const ContextT &ctx, const char *begin,
             std::span<HiddenLeafTraceEntry> traceEntries) {
      { ctx.trace_skip(begin, traceEntries) } noexcept ->
          std::same_as<SkipTraceResult>;
    };

using SkipHiddenNodesFn =
    const char *(*)(const void *, const char *, CstBuilder &) noexcept;
using SkipWithoutBuilderFn =
    const char *(*)(const void *, const char *) noexcept;
using TraceSkipFn =
    SkipTraceResult (*)(const void *, const char *,
                        std::span<HiddenLeafTraceEntry>) noexcept;

template <typename ContextT>
  requires ParseContextType<ContextT>
[[nodiscard]] const char *skip_hidden_nodes_for_context(
    const void *contextPtr, const char *begin, CstBuilder &builder) noexcept {
  return static_cast<const ContextT *>(contextPtr)->skip(begin, builder);
}

template <SkipWithoutBuilderContextType ContextT>
[[nodiscard]] const char *
skip_without_builder_for_context(const void *contextPtr,
                                 const char *begin) noexcept {
  return static_cast<const ContextT *>(contextPtr)->skip(begin);
}

template <SkipTraceContextType ContextT>
[[nodiscard]] SkipTraceResult
trace_skip_for_context(const void *contextPtr, const char *begin,
                       std::span<HiddenLeafTraceEntry> traceEntries) noexcept {
  return static_cast<const ContextT *>(contextPtr)->trace_skip(begin,
                                                               traceEntries);
}

struct Skipper {
  const void *context = nullptr;
  SkipHiddenNodesFn skipFn = &default_skip;
  SkipWithoutBuilderFn skipWithoutBuilderFn = &default_skip_without_builder;
  TraceSkipFn traceSkipFn = &default_trace_skip;
  std::shared_ptr<const void> owner;

  [[nodiscard]] static const char *default_skip(const void *,
                                                const char *begin,
                                                CstBuilder &) noexcept {
    return begin;
  }
  [[nodiscard]] static const char *
  default_skip_without_builder(const void *, const char *begin) noexcept {
    return begin;
  }

  [[nodiscard]] static SkipTraceResult
  default_trace_skip(const void *, const char *begin,
                     std::span<HiddenLeafTraceEntry>) noexcept {
    return {.end = begin};
  }

  template <typename ContextT>
  [[nodiscard]] static consteval TraceSkipFn trace_skip_fn_for() noexcept {
    if constexpr (SkipTraceContextType<ContextT>) {
      return &trace_skip_for_context<ContextT>;
    } else {
      return &default_trace_skip;
    }
  }

  template <typename ContextT>
    requires ParseContextType<ContextT>
  [[nodiscard]] static Skipper from(const ContextT &contextInstance) noexcept {
    return {std::addressof(contextInstance),
            &skip_hidden_nodes_for_context<ContextT>,
            &skip_without_builder_for_context<ContextT>,
            trace_skip_fn_for<ContextT>(),
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
            trace_skip_fn_for<StoredContext>(),
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

  [[nodiscard]] SkipTraceResult
  trace_skip(const char *begin,
             std::span<HiddenLeafTraceEntry> traceEntries) const noexcept {
    return traceSkipFn(context, begin, traceEntries);
  }

  [[nodiscard]] bool supports_trace_skip() const noexcept {
    return traceSkipFn != &default_trace_skip;
  }

  [[nodiscard]] const char *skip(const std::string &text) const noexcept {
    return skip(text.c_str());
  }

};

namespace detail {
inline const Skipper noOpSkipper{};
} // namespace detail

[[nodiscard]] inline const Skipper &NoOpSkipper() noexcept {
  return detail::noOpSkipper;
}

} // namespace pegium::parser
