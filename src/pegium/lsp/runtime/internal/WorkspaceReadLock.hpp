#pragma once

#include <cassert>
#include <optional>
#include <type_traits>
#include <utility>

#include <pegium/core/workspace/WorkspaceLock.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium {

/// Runs `action` under the workspace read lock and returns its result. The
/// shared workspace lock must be installed (asserted).
template <typename F>
decltype(auto)
with_workspace_read_lock(const pegium::SharedServices &sharedServices,
                         F &&action) {
  auto *lock = sharedServices.workspace.workspaceLock.get();
  assert(lock != nullptr);

  using Result = std::invoke_result_t<F>;
  if constexpr (std::is_void_v<Result>) {
    lock->read([task = std::forward<F>(action)]() mutable { task(); }).get();
    return;
  } else {
    std::optional<Result> result;
    lock->read([&result, task = std::forward<F>(action)]() mutable {
      result.emplace(task());
    }).get();
    Result value = std::move(result).value();
    return value;
  }
}

} // namespace pegium
