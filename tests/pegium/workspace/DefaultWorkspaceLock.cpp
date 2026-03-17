#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/workspace/DefaultWorkspaceLock.hpp>

namespace pegium::workspace {
namespace {

TEST(DefaultWorkspaceLockTest, NewWriteCancelsPreviousWrite) {
  DefaultWorkspaceLock lock;

  std::promise<void> firstStarted;
  auto firstStartedFuture = firstStarted.get_future();
  std::atomic<bool> firstCancelled = false;
  std::atomic<bool> secondExecuted = false;

  auto first = lock.write([&](const utils::CancellationToken &cancelToken) {
    firstStarted.set_value();
    while (!cancelToken.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    firstCancelled = true;
  });

  ASSERT_EQ(firstStartedFuture.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);

  auto second = lock.write([&](const utils::CancellationToken &cancelToken) {
    EXPECT_FALSE(cancelToken.stop_requested());
    secondExecuted = true;
  });

  first.get();
  second.get();

  EXPECT_TRUE(firstCancelled.load());
  EXPECT_TRUE(secondExecuted.load());
}

TEST(DefaultWorkspaceLockTest, ReadsObserveCompletedWrites) {
  DefaultWorkspaceLock lock;

  int value = 0;
  int observed = 0;

  auto write = lock.write([&](const utils::CancellationToken &) { value = 42; });
  auto read = lock.read([&]() { observed = value; });

  write.get();
  read.get();

  EXPECT_EQ(observed, 42);
}

TEST(DefaultWorkspaceLockTest, ReadActionResultCanBeAwaited) {
  DefaultWorkspaceLock lock;

  auto future = lock.read([]() -> void {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  });

  EXPECT_NO_THROW(future.get());
}

TEST(DefaultWorkspaceLockTest, CancelWriteStopsCurrentWriteAction) {
  DefaultWorkspaceLock lock;

  std::promise<void> started;
  auto startedFuture = started.get_future();
  std::atomic<bool> observedCancellation = false;

  auto future = lock.write([&](const utils::CancellationToken &token) {
    started.set_value();
    while (!token.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    observedCancellation = true;
  });

  ASSERT_EQ(startedFuture.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);
  lock.cancelWrite();
  future.get();

  EXPECT_TRUE(observedCancellation.load());
}

} // namespace
} // namespace pegium::workspace
