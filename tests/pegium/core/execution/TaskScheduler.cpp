#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <vector>

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

TEST(TaskSchedulerTest, ParallelForRunsInlineWithoutWorkers) {
  TaskScheduler scheduler(0);
  std::atomic<long> sum{0};
  std::vector<int> values(100);
  for (int i = 0; i < 100; ++i) {
    values[i] = i;
  }

  scheduler.parallelFor({}, values, [&](int value) {
    sum.fetch_add(value, std::memory_order_relaxed);
  });

  EXPECT_EQ(sum.load(std::memory_order_relaxed), 4950);
}

TEST(TaskSchedulerTest, ParallelForProcessesEveryItemExactlyOnce) {
  // Stresses the work distribution: with many items and many workers, every
  // index must be visited exactly once across repeated runs.
  TaskScheduler scheduler(8);
  constexpr int kCount = 50000;
  std::vector<std::atomic<int>> visits(kCount);
  for (auto &v : visits) {
    v.store(0, std::memory_order_relaxed);
  }
  std::vector<int> values(kCount);
  for (int i = 0; i < kCount; ++i) {
    values[i] = i;
  }

  for (int repeat = 0; repeat < 20; ++repeat) {
    scheduler.parallelFor({}, values, [&](int value) {
      visits[value].fetch_add(1, std::memory_order_relaxed);
    });
  }

  for (int i = 0; i < kCount; ++i) {
    ASSERT_EQ(visits[i].load(std::memory_order_relaxed), 20) << "index " << i;
  }
}

TEST(TaskSchedulerTest, ParallelForPropagatesTaskExceptions) {
  TaskScheduler scheduler(4);
  std::vector<int> values(1000);
  for (int i = 0; i < 1000; ++i) {
    values[i] = i;
  }

  EXPECT_THROW(
      scheduler.parallelFor({}, values,
                            [](int value) {
                              if (value == 500) {
                                throw std::runtime_error("boom");
                              }
                            }),
      std::runtime_error);
}

} // namespace
} // namespace pegium::execution
