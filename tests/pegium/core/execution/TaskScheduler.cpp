#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <stdexcept>
#include <thread>

#include <pegium/core/execution/TaskScheduler.hpp>

namespace pegium::execution {
namespace {

TEST(TaskSchedulerTest, ParallelForRunsSingleItemInline) {
  TaskScheduler scheduler(2);
  const auto currentThread = std::this_thread::get_id();
  std::thread::id observedThread;

  scheduler.parallelFor({}, std::array<int, 1>{1}, [&](int) {
    observedThread = std::this_thread::get_id();
  });

  EXPECT_EQ(observedThread, currentThread);
}

TEST(TaskSchedulerTest, SuggestedTaskCountHonorsMinimumChunkSize) {
  TaskScheduler scheduler(3);

  EXPECT_EQ(scheduler.suggestedTaskCount(0, 8), 0u);
  EXPECT_EQ(scheduler.suggestedTaskCount(1, 8), 1u);
  EXPECT_EQ(scheduler.suggestedTaskCount(7, 8), 1u);
  EXPECT_EQ(scheduler.suggestedTaskCount(8, 8), 1u);
  EXPECT_EQ(scheduler.suggestedTaskCount(9, 8), 2u);
  EXPECT_EQ(scheduler.suggestedTaskCount(80, 8), 10u);
}

TEST(TaskSchedulerTest, ScopeWaitsForNestedTasks) {
  TaskScheduler scheduler(2);
  std::atomic<int> sum = 0;

  scheduler.scope({}, [&](TaskScheduler::Scope &scope) {
    scope.spawn([&](TaskScheduler::Scope &childScope) {
      childScope.scope([&](TaskScheduler::Scope &nestedScope) {
        nestedScope.parallelFor(std::array<int, 4>{1, 2, 3, 4},
                                [&](int value) {
                                  sum.fetch_add(value, std::memory_order_relaxed);
                                });
      });
    });
  });

  EXPECT_EQ(sum.load(std::memory_order_relaxed), 10);
}

TEST(TaskSchedulerTest, ParallelForTasksCanSpawnNestedTasks) {
  TaskScheduler scheduler(2);
  std::atomic<int> sum = 0;

  scheduler.parallelFor({}, std::array<int, 4>{1, 2, 3, 4},
                        [&](TaskScheduler::Scope &scope, int value) {
                          scope.spawn([&sum, value] {
                            sum.fetch_add(value, std::memory_order_relaxed);
                          });
                        });

  EXPECT_EQ(sum.load(std::memory_order_relaxed), 10);
}

TEST(TaskSchedulerTest, JoinPropagatesTaskExceptions) {
  TaskScheduler scheduler(2);

  EXPECT_THROW(
      scheduler.scope({}, [&](TaskScheduler::Scope &scope) {
        scope.spawn([] {
          throw std::runtime_error("boom");
        });
      }),
      std::runtime_error);
}

} // namespace
} // namespace pegium::execution
