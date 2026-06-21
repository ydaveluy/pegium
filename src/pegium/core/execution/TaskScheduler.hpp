#pragma once

#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <ranges>
#include <utility>

#include <pegium/core/utils/Cancellation.hpp>

namespace pegium::execution {

/// Minimal parallel-for facade over a work-stealing executor, used by the
/// document builder to run each build phase across the workspace. The executor
/// type is kept out of this header (PIMPL) so it stays a private dependency.
class TaskScheduler {
public:
  explicit TaskScheduler(std::size_t workerCount = defaultWorkerCount());
  ~TaskScheduler() noexcept;

  TaskScheduler(const TaskScheduler &) = delete;
  TaskScheduler &operator=(const TaskScheduler &) = delete;
  TaskScheduler(TaskScheduler &&) = delete;
  TaskScheduler &operator=(TaskScheduler &&) = delete;

  /// Default worker thread count: one fewer than the hardware concurrency so the
  /// submitting thread is not oversubscribed (0 when concurrency is unknown or
  /// 1, in which case parallelFor runs inline on the caller).
  [[nodiscard]] static std::size_t defaultWorkerCount() noexcept;

  /// Applies @p task to every element of @p range in parallel and waits for all
  /// of them. The first exception any task throws, and cooperative cancellation
  /// via @p cancelToken, propagate to the caller. A single element (and the
  /// no-worker configuration) runs inline on the calling thread.
  template <typename Range, typename F>
  void parallelFor(const utils::CancellationToken &cancelToken, Range &&range,
                   F &&task);

private:
  struct Impl;

  // Runs body(i) for i in [0, count) in parallel on the executor, capturing the
  // first exception thrown and honouring cooperative cancellation. count > 1 and
  // a non-null executor are guaranteed by the caller.
  void parallelForIndexed(const utils::CancellationToken &cancelToken,
                          std::size_t count,
                          const std::function<void(std::size_t)> &body);

  std::unique_ptr<Impl> _impl; // null when there are no workers
};

template <typename Range, typename F>
void TaskScheduler::parallelFor(const utils::CancellationToken &cancelToken,
                                Range &&range, F &&task) {
  utils::throw_if_cancelled(cancelToken);
  const auto begin = std::begin(range);
  const auto end = std::end(range);

  if constexpr (std::ranges::random_access_range<Range> &&
                std::ranges::sized_range<Range>) {
    const auto count = static_cast<std::size_t>(std::ranges::size(range));
    if (count == 0U) {
      return;
    }
    // Dispatch to the executor only when there is more than one element and
    // worker threads exist; otherwise run inline (a single element stays on the
    // calling thread, and the no-worker config has nowhere to dispatch).
    if (_impl != nullptr && count > 1U) {
      parallelForIndexed(cancelToken, count, [&](std::size_t index) {
        task(*(begin + static_cast<std::ptrdiff_t>(index)));
      });
      return;
    }
  }

  for (auto current = begin; current != end; ++current) {
    utils::throw_if_cancelled(cancelToken);
    task(*current);
  }
}

} // namespace pegium::execution
