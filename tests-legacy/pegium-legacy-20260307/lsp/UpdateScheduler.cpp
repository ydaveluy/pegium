#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <pegium/lsp/DefaultUpdateScheduler.hpp>
#include <pegium/utils/Cancellation.hpp>

namespace {

TEST(ArchitectureTest, UpdateSchedulerExecutesTasksInFifoOrder) {
  pegium::lsp::DefaultUpdateScheduler scheduler;
  std::vector<int> order;
  std::mutex orderMutex;

  auto first = scheduler.enqueue([&](const pegium::utils::CancellationToken &) {
    std::scoped_lock lock(orderMutex);
    order.push_back(1);
  });
  auto second = scheduler.enqueue(
      [&](const pegium::utils::CancellationToken &) {
        std::scoped_lock lock(orderMutex);
        order.push_back(2);
      },
      {.uri = std::string("file:///a.pg")});
  auto third = scheduler.enqueue([&](const pegium::utils::CancellationToken &) {
    std::scoped_lock lock(orderMutex);
    order.push_back(3);
  });

  first.get();
  second.get();
  third.get();
  EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

TEST(ArchitectureTest, UpdateSchedulerSupersedesSameUriPendingAndInFlightTasks) {
  pegium::lsp::DefaultUpdateScheduler scheduler;
  std::promise<void> firstStarted;
  auto started = firstStarted.get_future();
  std::atomic<int> ranSecond{0};

  auto first = scheduler.enqueue(
      [&](const pegium::utils::CancellationToken &cancelToken) {
        firstStarted.set_value();
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!cancelToken.stop_requested()) {
          if (std::chrono::steady_clock::now() >= deadline) {
            throw std::runtime_error("superseding did not cancel in-flight task");
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        pegium::utils::throw_if_cancelled(cancelToken);
      },
      {.uri = std::string("file:///same.pg"), .supersedeUri = true});

  ASSERT_EQ(started.wait_for(std::chrono::seconds(1)), std::future_status::ready);

  auto second = scheduler.enqueue(
      [&](const pegium::utils::CancellationToken &) { ++ranSecond; },
      {.uri = std::string("file:///same.pg"), .supersedeUri = true});

  EXPECT_NO_THROW(first.get());
  EXPECT_NO_THROW(second.get());
  EXPECT_EQ(ranSecond.load(), 1);
}

TEST(ArchitectureTest, UpdateSchedulerWaitUntilAppliedUsesGlobalSequenceFence) {
  pegium::lsp::DefaultUpdateScheduler scheduler;
  std::atomic<int> executed{0};

  auto first = scheduler.enqueue([&](const pegium::utils::CancellationToken &) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ++executed;
  });
  auto second =
      scheduler.enqueue([&](const pegium::utils::CancellationToken &) { ++executed; });
  const auto watermark = scheduler.lastEnqueuedSeq();

  EXPECT_TRUE(scheduler.waitUntilApplied(watermark));
  EXPECT_NO_THROW(first.get());
  EXPECT_NO_THROW(second.get());
  EXPECT_EQ(executed.load(), 2);
}

TEST(ArchitectureTest,
     UpdateSchedulerWaitUntilAppliedWaitsForCancelledInFlightTaskToFinish) {
  pegium::lsp::DefaultUpdateScheduler scheduler;
  std::promise<void> startedPromise;
  auto started = startedPromise.get_future();
  std::atomic<bool> firstCancelled{false};

  auto first = scheduler.enqueue(
      [&](const pegium::utils::CancellationToken &cancelToken) {
        startedPromise.set_value();
        while (!cancelToken.stop_requested()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        firstCancelled.store(true);
        pegium::utils::throw_if_cancelled(cancelToken);
      },
      {.uri = std::string("file:///same.pg"), .supersedeUri = true});

  ASSERT_EQ(started.wait_for(std::chrono::seconds(1)), std::future_status::ready);
  auto second = scheduler.enqueue(
      [&](const pegium::utils::CancellationToken &) {},
      {.uri = std::string("file:///same.pg"), .supersedeUri = true});
  const auto fence = scheduler.lastEnqueuedSeq();

  scheduler.requestStop();
  EXPECT_TRUE(scheduler.waitUntilApplied(fence));
  EXPECT_NO_THROW(first.get());
  EXPECT_NO_THROW(second.get());
  EXPECT_TRUE(firstCancelled.load());
}

} // namespace
