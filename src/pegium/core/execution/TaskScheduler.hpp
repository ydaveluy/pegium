#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <ranges>
#include <stop_token>
#include <thread>
#include <vector>

#include <pegium/core/utils/Cancellation.hpp>

namespace pegium::execution {

/// Cooperative work-stealing scheduler used by shared background tasks.
class TaskScheduler {
public:
  class Scope;

  explicit TaskScheduler(std::size_t workerCount = defaultWorkerCount());
  ~TaskScheduler() noexcept;

  TaskScheduler(const TaskScheduler &) = delete;
  TaskScheduler &operator=(const TaskScheduler &) = delete;
  TaskScheduler(TaskScheduler &&) = delete;
  TaskScheduler &operator=(TaskScheduler &&) = delete;

  [[nodiscard]] static std::size_t defaultWorkerCount() noexcept;
  [[nodiscard]] std::size_t workerCount() const noexcept;
  [[nodiscard]] std::size_t
  suggestedTaskCount(std::size_t itemCount,
                     std::size_t minChunkSize = 1) const noexcept;

  template <typename F>
  void scope(const utils::CancellationToken &cancelToken, F &&body) {
    scopeWithExecutionContext(cancelToken, {}, std::forward<F>(body));
  }

  template <typename Range, typename F>
  void parallelFor(const utils::CancellationToken &cancelToken, Range &&range,
                   F &&task);

private:
  struct ExecutionContext {
    const TaskScheduler *scheduler;
    std::size_t workerIndex;

    constexpr ExecutionContext(
        const TaskScheduler *scheduler = nullptr,
        std::size_t workerIndex = invalidWorkerIndex()) noexcept
        : scheduler(scheduler), workerIndex(workerIndex) {}

    [[nodiscard]] static constexpr std::size_t
    invalidWorkerIndex() noexcept {
      return static_cast<std::size_t>(-1);
    }

    [[nodiscard]] constexpr bool
    isWorkerOf(const TaskScheduler &owner) const noexcept {
      return scheduler == &owner && workerIndex != invalidWorkerIndex();
    }
  };

  struct TaskGroupState;
  struct ScheduledTask;
  struct WorkerState;

  friend class Scope;

  template <typename F>
  static void invokeScopedTask(F &task, Scope &scope) {
    if constexpr (std::is_invocable_v<F &, Scope &>) {
      task(scope);
    } else if constexpr (std::is_invocable_v<F &>) {
      task();
    } else {
      static_assert(always_false_v<F>,
                    "Task must be invocable with Scope& or with no argument.");
    }
  }

  template <typename F, typename T>
  static void invokeParallelTask(F &task, Scope &scope, T &&value) {
    if constexpr (std::is_invocable_v<F &, Scope &, T>) {
      task(scope, std::forward<T>(value));
    } else if constexpr (std::is_invocable_v<F &, T>) {
      task(std::forward<T>(value));
    } else {
      static_assert(always_false_v<F>,
                    "Parallel task must be invocable with (Scope&, value) or "
                    "with (value).");
    }
  }

  template <typename F>
  void scopeWithExecutionContext(const utils::CancellationToken &cancelToken,
                                 ExecutionContext executionContext, F &&body) {
    auto sharedBody =
        std::make_shared<std::decay_t<F>>(std::forward<F>(body));
    run(cancelToken,
        [sharedBody](Scope &scope) mutable {
          invokeScopedTask(*sharedBody, scope);
        },
        executionContext);
  }

  void enqueue(const std::shared_ptr<TaskGroupState> &group,
               std::function<void(Scope &)> task,
               ExecutionContext executionContext = {});
  void join(const std::shared_ptr<TaskGroupState> &group,
            ExecutionContext executionContext = {});
  [[nodiscard]] Scope makeScope(const utils::CancellationToken &cancelToken,
                                ExecutionContext executionContext = {});
  void run(const utils::CancellationToken &cancelToken,
           const std::function<void(Scope &)> &body,
           ExecutionContext executionContext = {});

  [[nodiscard]] bool tryPopTask(ScheduledTask &task,
                                ExecutionContext executionContext);
  [[nodiscard]] bool tryPopTaskFromWorker(std::size_t workerIndex,
                                          ScheduledTask &task);
  [[nodiscard]] bool tryPopGlobalTask(ScheduledTask &task);
  [[nodiscard]] bool tryStealTask(std::size_t thiefIndex, ScheduledTask &task);
  void executeTask(ScheduledTask task, ExecutionContext executionContext);
  void workerLoop(std::size_t workerIndex, std::stop_token stopToken);

  std::vector<std::unique_ptr<WorkerState>> _workers;
  std::vector<std::jthread> _threads;
  mutable std::mutex _globalMutex;
  std::condition_variable _globalCv;
  std::deque<ScheduledTask> _globalQueue;
  std::atomic<bool> _stopping{false};

  template <typename>
  static inline constexpr bool always_false_v = false;
};

class TaskScheduler::Scope {
public:
  /// Returns the scheduler that owns this execution scope.
  [[nodiscard]] TaskScheduler &scheduler() const noexcept;
  [[nodiscard]] const utils::CancellationToken &
  cancellationToken() const noexcept;
  void throwIfCancelled() const;

  template <typename F> void spawn(F &&task) {
    if (_scheduler == nullptr) {
      return;
    }
    auto sharedTask =
        std::make_shared<std::decay_t<F>>(std::forward<F>(task));
    _scheduler->enqueue(
        _group, [sharedTask](Scope &scope) mutable {
          TaskScheduler::invokeScopedTask(*sharedTask, scope);
        },
        _executionContext);
  }

  template <typename Range, typename F>
  void parallelFor(Range &&range, F &&task) {
    auto begin = std::begin(range);
    const auto end = std::end(range);
    if (begin == end) {
      return;
    }

    auto sharedTask =
        std::make_shared<std::decay_t<F>>(std::forward<F>(task));

    if constexpr (std::ranges::random_access_range<Range> &&
                  std::ranges::sized_range<Range>) {
      const auto count = static_cast<std::size_t>(std::ranges::size(range));
      const auto chunkCount = _scheduler->suggestedTaskCount(count, 8);
      if (chunkCount <= 1) {
        TaskScheduler::invokeParallelTask(*sharedTask, *this, *begin);
        for (auto current = std::next(begin); current != end; ++current) {
          throwIfCancelled();
          TaskScheduler::invokeParallelTask(*sharedTask, *this, *current);
        }
        return;
      }

      const auto chunkSize = (count + chunkCount - 1U) / chunkCount;
      // Keep the first chunk on the caller thread to avoid queueing overhead on
      // tiny batches and to preserve the cheap single-document fast path.
      auto runChunk = [begin, count, chunkSize, sharedTask](Scope &scope,
                                                            std::size_t chunkIndex) mutable {
        const auto chunkBegin = chunkIndex * chunkSize;
        const auto chunkEnd = std::min(chunkBegin + chunkSize, count);
        for (auto index = chunkBegin; index < chunkEnd; ++index) {
          scope.throwIfCancelled();
          TaskScheduler::invokeParallelTask(
              *sharedTask, scope,
              *(begin + static_cast<std::ptrdiff_t>(index)));
        }
      };

      for (std::size_t chunkIndex = 1; chunkIndex < chunkCount; ++chunkIndex) {
        spawn([runChunk, chunkIndex](Scope &scope) mutable {
          runChunk(scope, chunkIndex);
        });
      }

      runChunk(*this, 0);
      return;
    }

    auto current = begin;
    ++current;
    for (; current != end; ++current) {
      auto value = *current;
      spawn([sharedTask, value](Scope &scope) mutable {
        TaskScheduler::invokeParallelTask(*sharedTask, scope, value);
      });
    }

    TaskScheduler::invokeParallelTask(*sharedTask, *this, *begin);
  }

  template <typename F> void scope(F &&body) const {
    if (_scheduler == nullptr) {
      return;
    }
    _scheduler->scopeWithExecutionContext(cancellationToken(),
                                          _executionContext,
                                          std::forward<F>(body));
  }

private:
  friend class TaskScheduler;

  Scope(TaskScheduler &scheduler, std::shared_ptr<TaskGroupState> group) noexcept;
  Scope(TaskScheduler &scheduler, std::shared_ptr<TaskGroupState> group,
        ExecutionContext executionContext) noexcept;

  TaskScheduler *_scheduler = nullptr;
  std::shared_ptr<TaskGroupState> _group;
  ExecutionContext _executionContext{};
};

template <typename Range, typename F>
void TaskScheduler::parallelFor(const utils::CancellationToken &cancelToken,
                                Range &&range, F &&task) {
  scope(cancelToken, [&range, &task](Scope &scope) {
    scope.parallelFor(std::forward<Range>(range), std::forward<F>(task));
  });
}

} // namespace pegium::execution
