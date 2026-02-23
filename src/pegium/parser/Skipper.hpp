#pragma once

#include <concepts>
#include <memory>
#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/syntax-tree/CstBuilder.hpp>
#include <string_view>
#include <type_traits>
#include <utility>

namespace pegium::parser {

template <typename T>
concept ParseContextType = requires(const T &ctx, const char *begin,
                                    const char *end, CstBuilder &builder) {
  {
    ctx.skipHiddenNodes(begin, end, builder)
  } noexcept -> std::same_as<const char *>;
};

template <typename T>
concept RecoveryPolicyContextType = requires(
    const T &ctx, const grammar::AbstractElement *element, const char *cursor,
    const char *end) {
  {
    ctx.canForceInsert(element, cursor, end)
  } noexcept -> std::same_as<bool>;
};

using SkipHiddenNodesFn = const char *(*)(const void *, const char *,
                                          const char *,
                                          CstBuilder &) noexcept;
using CanForceInsertFn = bool (*)(const void *,
                                  const grammar::AbstractElement *,
                                  const char *, const char *) noexcept;

template <typename Context>
  requires ParseContextType<Context>
[[nodiscard]] const char *skip_hidden_nodes(const void *context,
                                            const char *begin, const char *end,
                                            CstBuilder &builder) noexcept {
  return static_cast<const Context *>(context)->skipHiddenNodes(begin, end,
                                                                builder);
}

template <typename Context>
  requires RecoveryPolicyContextType<Context>
[[nodiscard]] bool can_force_insert(const void *context,
                                    const grammar::AbstractElement *element,
                                    const char *cursor,
                                    const char *end) noexcept {
  return static_cast<const Context *>(context)->canForceInsert(element, cursor,
                                                               end);
}

struct Skipper {
  const void *context = nullptr;
  SkipHiddenNodesFn skip = &default_skip;
  CanForceInsertFn canForceInsertFn = &default_can_force_insert;
  std::shared_ptr<const void> owner;

  [[nodiscard]] static const char *default_skip(const void *, const char *begin,
                                                const char *,
                                                CstBuilder &) noexcept {
    return begin;
  }
  [[nodiscard]] static bool
  default_can_force_insert(const void *, const grammar::AbstractElement *,
                           const char *, const char *) noexcept {
    return false;
  }

  template <typename Context>
    requires ParseContextType<Context>
  [[nodiscard]] static Skipper from(const Context &context) noexcept {
    if constexpr (RecoveryPolicyContextType<Context>) {
      return {&context, &skip_hidden_nodes<Context>, &can_force_insert<Context>,
              {}};
    } else {
      return {&context, &skip_hidden_nodes<Context>, &default_can_force_insert,
              {}};
    }
  }

  template <typename Context>
    requires ParseContextType<std::remove_cvref_t<Context>>
  [[nodiscard]] static Skipper owning(Context &&context) {
    using StoredContext = std::remove_cvref_t<Context>;
    auto owned =
        std::make_shared<StoredContext>(std::forward<Context>(context));
    if constexpr (RecoveryPolicyContextType<StoredContext>) {
      return {owned.get(),
              &skip_hidden_nodes<StoredContext>,
              &can_force_insert<StoredContext>,
              std::move(owned)};
    } else {
      return {owned.get(),
              &skip_hidden_nodes<StoredContext>,
              &default_can_force_insert,
              std::move(owned)};
    }
  }

  [[nodiscard]] const char *
  skipHiddenNodes(const char *begin, const char *end,
                  CstBuilder &builder) const noexcept {
    return skip(context, begin, end, builder);
  }

  [[nodiscard]] MatchResult
  skipHiddenNodes(std::string_view sv, CstBuilder &builder) const noexcept {
    return MatchResult::success(skipHiddenNodes(sv.begin(), sv.end(), builder));
  }

  [[nodiscard]] bool
  canForceInsert(const grammar::AbstractElement *element, const char *cursor,
                 const char *end) const noexcept {
    return canForceInsertFn(context, element, cursor, end);
  }
};

} // namespace pegium::parser
