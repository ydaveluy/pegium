#include <pegium/execution/TaskScheduler.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <stdexcept>
#include <utility>

namespace pegium::execution {

struct TaskScheduler::TaskGroupState {
  explicit TaskGroupState(const utils::CancellationToken &cancelToken)
      : cancelToken(cancelToken) {}

  mutable std::mutex mutex;
  std::condition_variable cv;
  std::atomic<std::size_t> pending{0};
  std::atomic<bool> cancelled{false};
  utils::CancellationToken cancelToken;
  std::exception_ptr exception;
};

struct TaskScheduler::ScheduledTask {
  std::shared_ptr<TaskGroupState> group;
  std::function<void(Scope &)> task;
};

struct TaskScheduler::WorkerState {
  mutable std::mutex mutex;
  std::deque<ScheduledTask> queue;
};

TaskScheduler::TaskScheduler(std::size_t workerCount) {
  _workers.reserve(workerCount);
  _threads.reserve(workerCount);
  for (std::size_t index = 0; index < workerCount; ++index) {
    _workers.push_back(std::make_unique<WorkerState>());
  }
  for (std::size_t index = 0; index < workerCount; ++index) {
    _threads.emplace_back(
        [this, index](std::stop_token stopToken) { workerLoop(index, stopToken); });
  }
}

TaskScheduler::~TaskScheduler() noexcept {
  _stopping.store(true);
  _globalCv.notify_all();
}

std::size_t TaskScheduler::defaultWorkerCount() noexcept {
  const auto concurrency = std::thread::hardware_concurrency();
  if (concurrency <= 1U) {
    return 0;
  }
  return static_cast<std::size_t>(concurrency - 1U);
}

std::size_t TaskScheduler::workerCount() const noexcept {
  return _workers.size();
}

std::size_t
TaskScheduler::suggestedTaskCount(std::size_t itemCount,
                                  std::size_t minChunkSize) const noexcept {
  if (itemCount == 0U) {
    return 0U;
  }
  if (minChunkSize == 0U) {
    minChunkSize = 1U;
  }

  const auto participants = workerCount() + 1U;
  const auto targetTasks = std::max<std::size_t>(1U, participants * 4U);
  const auto maxTasksByChunkSize =
      std::max<std::size_t>(1U, (itemCount + minChunkSize - 1U) / minChunkSize);
  return std::min(targetTasks, maxTasksByChunkSize);
}

TaskScheduler::Scope
TaskScheduler::makeScope(const utils::CancellationToken &cancelToken,
                         ExecutionContext executionContext) {
  return Scope(*this, std::make_shared<TaskGroupState>(cancelToken),
               executionContext);
}

void TaskScheduler::run(const utils::CancellationToken &cancelToken,
                        const std::function<void(Scope &)> &body,
                        ExecutionContext executionContext) {
  utils::throw_if_cancelled(cancelToken);
  if (!body) {
    return;
  }

  auto scope = makeScope(cancelToken, executionContext);
  std::exception_ptr bodyException;
  try {
    body(scope);
  } catch (...) {
    bodyException = std::current_exception();
    if (scope._group) {
      std::scoped_lock lock(scope._group->mutex);
      if (scope._group->exception == nullptr) {
        scope._group->exception = bodyException;
      }
      scope._group->cancelled.store(true);
    }
  }

  try {
    join(scope._group, executionContext);
  } catch (...) {
    if (bodyException == nullptr) {
      throw;
    }
  }

  if (bodyException != nullptr) {
    std::rethrow_exception(bodyException);
  }
  utils::throw_if_cancelled(cancelToken);
}

void TaskScheduler::enqueue(const std::shared_ptr<TaskGroupState> &group,
                            std::function<void(Scope &)> task,
                            ExecutionContext executionContext) {
  if (!group || !task) {
    return;
  }

  group->pending.fetch_add(1, std::memory_order_relaxed);
  ScheduledTask scheduled{.group = group, .task = std::move(task)};

  if (executionContext.isWorkerOf(*this) &&
      executionContext.workerIndex < _workers.size()) {
    auto &worker = *_workers[executionContext.workerIndex];
    std::scoped_lock lock(worker.mutex);
    worker.queue.push_back(std::move(scheduled));
  } else {
    std::scoped_lock lock(_globalMutex);
    _globalQueue.push_back(std::move(scheduled));
  }
  _globalCv.notify_one();
}

void TaskScheduler::join(const std::shared_ptr<TaskGroupState> &group,
                         ExecutionContext executionContext) {
  if (!group) {
    return;
  }

  while (group->pending.load(std::memory_order_acquire) != 0U) {
    if (ScheduledTask task; tryPopTask(task, executionContext)) {
      executeTask(std::move(task), executionContext);
      continue;
    }

    std::unique_lock lock(group->mutex);
    if (group->pending.load(std::memory_order_acquire) == 0U) {
      break;
    }
    group->cv.wait_for(lock, std::chrono::milliseconds(1));
  }

  if (group->exception != nullptr) {
    std::rethrow_exception(group->exception);
  }
  utils::throw_if_cancelled(group->cancelToken);
}

bool TaskScheduler::tryPopTask(ScheduledTask &task,
                               ExecutionContext executionContext) {
  if (executionContext.isWorkerOf(*this) &&
      executionContext.workerIndex < _workers.size()) {
    if (tryPopTaskFromWorker(executionContext.workerIndex, task)) {
      return true;
    }
    if (tryPopGlobalTask(task)) {
      return true;
    }
    return tryStealTask(executionContext.workerIndex, task);
  }

  if (tryPopGlobalTask(task)) {
    return true;
  }
  return tryStealTask(ExecutionContext::invalidWorkerIndex(), task);
}

bool TaskScheduler::tryPopTaskFromWorker(std::size_t workerIndex,
                                         ScheduledTask &task) {
  if (workerIndex >= _workers.size()) {
    return false;
  }
  auto &worker = *_workers[workerIndex];
  std::scoped_lock lock(worker.mutex);
  if (worker.queue.empty()) {
    return false;
  }
  task = std::move(worker.queue.back());
  worker.queue.pop_back();
  return true;
}

bool TaskScheduler::tryPopGlobalTask(ScheduledTask &task) {
  std::scoped_lock lock(_globalMutex);
  if (_globalQueue.empty()) {
    return false;
  }
  task = std::move(_globalQueue.front());
  _globalQueue.pop_front();
  return true;
}

bool TaskScheduler::tryStealTask(std::size_t thiefIndex, ScheduledTask &task) {
  for (std::size_t index = 0; index < _workers.size(); ++index) {
    if (index == thiefIndex) {
      continue;
    }
    auto &worker = *_workers[index];
    std::scoped_lock lock(worker.mutex);
    if (worker.queue.empty()) {
      continue;
    }
    task = std::move(worker.queue.front());
    worker.queue.pop_front();
    return true;
  }
  return false;
}

void TaskScheduler::executeTask(ScheduledTask task,
                                ExecutionContext executionContext) {
  if (!task.group || !task.task) {
    return;
  }

  try {
    if (task.group->cancelToken.stop_requested() ||
        task.group->cancelled.load()) {
      throw utils::OperationCancelled();
    }
    Scope scope(*this, task.group, executionContext);
    task.task(scope);
    scope.throwIfCancelled();
  } catch (const utils::OperationCancelled &) {
    task.group->cancelled.store(true);
    std::scoped_lock lock(task.group->mutex);
    if (task.group->exception == nullptr &&
        task.group->cancelToken.stop_requested()) {
      task.group->exception = std::make_exception_ptr(utils::OperationCancelled());
    }
  } catch (...) {
    task.group->cancelled.store(true);
    std::scoped_lock lock(task.group->mutex);
    if (task.group->exception == nullptr) {
      task.group->exception = std::current_exception();
    }
  }

  task.group->pending.fetch_sub(1, std::memory_order_acq_rel);
  std::scoped_lock lock(task.group->mutex);
  task.group->cv.notify_all();
}

void TaskScheduler::workerLoop(std::size_t workerIndex,
                               std::stop_token stopToken) {
  const ExecutionContext executionContext(this, workerIndex);

  while (!stopToken.stop_requested() && !_stopping.load()) {
    if (ScheduledTask task; tryPopTask(task, executionContext)) {
      executeTask(std::move(task), executionContext);
      continue;
    }

    std::unique_lock lock(_globalMutex);
    _globalCv.wait_for(lock, std::chrono::milliseconds(1), [this, &stopToken]() {
      return stopToken.stop_requested() || _stopping.load() || !_globalQueue.empty();
    });
  }
}

TaskScheduler::Scope::Scope(
    TaskScheduler &scheduler, std::shared_ptr<TaskGroupState> group) noexcept
    : Scope(scheduler, std::move(group), {}) {}

TaskScheduler::Scope::Scope(
    TaskScheduler &scheduler, std::shared_ptr<TaskGroupState> group,
    ExecutionContext executionContext) noexcept
    : _scheduler(&scheduler), _group(std::move(group)),
      _executionContext(executionContext) {}

TaskScheduler &TaskScheduler::Scope::scheduler() const noexcept {
  return *_scheduler;
}

const utils::CancellationToken &
TaskScheduler::Scope::cancellationToken() const noexcept {
  return _group->cancelToken;
}

void TaskScheduler::Scope::throwIfCancelled() const {
  if (_group->cancelToken.stop_requested() || _group->cancelled.load()) {
    throw utils::OperationCancelled();
  }
}

} // namespace pegium::execution
