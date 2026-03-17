#pragma once

#include <functional>
#include <future>
#include <optional>
#include <type_traits>
#include <utility>

#include <pegium/utils/Cancellation.hpp>

namespace pegium::workspace {

class WorkspaceLock {
public:
  using WriteAction =
      std::function<void(const utils::CancellationToken &cancelToken)>;
  using ReadAction = std::function<void()>;

  virtual ~WorkspaceLock() noexcept = default;
  [[nodiscard]] virtual std::future<void> write(WriteAction action) = 0;
  [[nodiscard]] virtual std::future<void> read(ReadAction action) = 0;
  virtual void cancelWrite() = 0;
};

template <typename F>
auto run_with_workspace_read(WorkspaceLock *lock, F &&action)
    -> std::invoke_result_t<F> {
  using Result = std::invoke_result_t<F>;
  if (lock == nullptr) {
    if constexpr (std::is_void_v<Result>) {
      std::forward<F>(action)();
      return;
    } else {
      return std::forward<F>(action)();
    }
  }

  if constexpr (std::is_void_v<Result>) {
    lock->read([task = std::forward<F>(action)]() mutable { task(); }).get();
    return;
  } else {
    std::optional<Result> result;
    lock->read([&result, task = std::forward<F>(action)]() mutable {
      result.emplace(task());
    }).get();
    return std::move(*result);
  }
}

template <typename F>
auto run_with_workspace_write(WorkspaceLock *lock,
                              const utils::CancellationToken &cancelToken,
                              F &&action) -> std::invoke_result_t<F> {
  using Result = std::invoke_result_t<F>;
  utils::throw_if_cancelled(cancelToken);
  if (lock == nullptr) {
    if constexpr (std::is_void_v<Result>) {
      std::forward<F>(action)();
      utils::throw_if_cancelled(cancelToken);
      return;
    } else {
      auto result = std::forward<F>(action)();
      utils::throw_if_cancelled(cancelToken);
      return result;
    }
  }

  if constexpr (std::is_void_v<Result>) {
    lock
        ->write([&cancelToken, task = std::forward<F>(action)](
                    const utils::CancellationToken &lockToken) mutable {
          utils::throw_if_cancelled(cancelToken);
          utils::throw_if_cancelled(lockToken);
          task();
          utils::throw_if_cancelled(cancelToken);
          utils::throw_if_cancelled(lockToken);
        })
        .get();
    return;
  } else {
    std::optional<Result> result;
    lock
        ->write([&cancelToken, &result, task = std::forward<F>(action)](
                    const utils::CancellationToken &lockToken) mutable {
          utils::throw_if_cancelled(cancelToken);
          utils::throw_if_cancelled(lockToken);
          result.emplace(task());
          utils::throw_if_cancelled(cancelToken);
          utils::throw_if_cancelled(lockToken);
        })
        .get();
    utils::throw_if_cancelled(cancelToken);
    if (!result.has_value()) {
      throw utils::OperationCancelled();
    }
    return std::move(*result);
  }
}

} // namespace pegium::workspace
