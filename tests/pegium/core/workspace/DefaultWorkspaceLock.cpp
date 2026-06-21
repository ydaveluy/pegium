#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <optional>
#include <thread>
#include <vector>

#include <pegium/core/CoreTestSupport.hpp>
#include <pegium/core/workspace/DefaultWorkspaceLock.hpp>

namespace pegium::workspace {
namespace {

TEST(DefaultWorkspaceLockTest, WriteActionsAreExecutedSequentially) {
  DefaultWorkspaceLock lock;

  constexpr std::size_t kActionCount = 5;
  std::atomic<int> counter = 0;
  std::vector<std::promise<void>> gates;
  std::vector<std::shared_future<void>> gateFutures;
  std::vector<std::future<void>> futures;
  gates.reserve(kActionCount);
  gateFutures.reserve(kActionCount);
  futures.reserve(kActionCount);

  for (std::size_t index = 0; index < kActionCount; ++index) {
    gates.emplace_back();
    gateFutures.push_back(gates.back().get_future().share());
  }

  for (std::size_t index = 0; index < kActionCount; ++index) {
    futures.push_back(lock.write([&, index](const utils::CancellationToken &,
                                            const auto &) {
      ++counter;
      gateFutures[index].wait();
    }));
  }

  for (std::size_t index = 0; index < kActionCount; ++index) {
    ASSERT_TRUE(test::wait_until(
        [&counter, index]() { return counter.load() == index + 1; }));
    gates[index].set_value();
  }

  for (auto &future : futures) {
    future.get();
  }
}

TEST(DefaultWorkspaceLockTest, NewWriteCancelsPreviousWrite) {
  DefaultWorkspaceLock lock;

  std::promise<void> firstStarted;
  auto firstStartedFuture = firstStarted.get_future();
  std::atomic<bool> firstCancelled = false;
  std::atomic<bool> secondExecuted = false;

  auto first = lock.write([&](const utils::CancellationToken &cancelToken, const auto &) {
    firstStarted.set_value();
    while (!cancelToken.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    firstCancelled = true;
  });

  ASSERT_EQ(firstStartedFuture.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);

  auto second = lock.write([&](const utils::CancellationToken &cancelToken, const auto &) {
    EXPECT_FALSE(cancelToken.stop_requested());
    secondExecuted = true;
  });

  first.get();
  second.get();

  EXPECT_TRUE(firstCancelled.load());
  EXPECT_TRUE(secondExecuted.load());
}

TEST(DefaultWorkspaceLockTest, WriteActionsHavePriorityOverQueuedReads) {
  DefaultWorkspaceLock lock;

  std::promise<void> releaseFirstWrite;
  auto releaseFirstWriteFuture = releaseFirstWrite.get_future().share();
  std::promise<void> secondWriteStarted;
  auto secondWriteStartedFuture = secondWriteStarted.get_future();
  std::atomic<int> counter = 0;

  auto firstWrite =
      lock.write([&](const utils::CancellationToken &, const auto &) {
        ++counter;
        releaseFirstWriteFuture.wait();
      });

  auto readResult = std::async(std::launch::async, [&]() {
    std::optional<int> result;
    auto future = lock.read([&counter, &result]() { result = counter.load(); });
    future.get();
    return *result;
  });

  auto secondWrite =
      lock.write([&](const utils::CancellationToken &, const auto &) {
        ++counter;
        secondWriteStarted.set_value();
      });

  ASSERT_TRUE(test::wait_until([&counter]() { return counter.load() == 1; }));
  releaseFirstWrite.set_value();
  ASSERT_EQ(secondWriteStartedFuture.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);

  EXPECT_EQ(counter.load(), 2);
  EXPECT_EQ(readResult.get(), 2);

  firstWrite.get();
  secondWrite.get();
}

TEST(DefaultWorkspaceLockTest, ReadsObserveCompletedWrites) {
  DefaultWorkspaceLock lock;

  int value = 0;
  int observed = 0;

  auto write = lock.write([&](const utils::CancellationToken &, const auto &) { value = 42; });
  auto read = lock.read([&]() { observed = value; });

  write.get();
  read.get();

  EXPECT_EQ(observed, 42);
}

TEST(DefaultWorkspaceLockTest, ReadActionResultCanBeAwaited) {
  DefaultWorkspaceLock lock;

  auto future = std::async(std::launch::async, [&]() {
    std::optional<int> result;
    auto read = lock.read([&result]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      result = 42;
    });
    read.get();
    return *result;
  });

  EXPECT_EQ(future.get(), 42);
}

TEST(DefaultWorkspaceLockTest, DowngradeLetsReadsRunWhileWriteTailContinues) {
  DefaultWorkspaceLock lock;

  std::promise<void> downgraded;
  auto downgradedFuture = downgraded.get_future();
  std::promise<void> releaseTail;
  auto releaseTailFuture = releaseTail.get_future().share();
  std::atomic<bool> readRan = false;

  auto write = lock.write([&](const utils::CancellationToken &,
                              const auto &downgrade) {
    // Exclusive part done — downgrade, then keep working as a reader (this models
    // validation running after the model is linked).
    downgrade();
    downgraded.set_value();
    releaseTailFuture.wait();
  });

  ASSERT_EQ(downgradedFuture.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);

  // The write is still running its (downgraded) tail. A read must run now,
  // concurrently, rather than block until the write returns.
  auto read = lock.read([&]() { readRan = true; });
  ASSERT_EQ(read.wait_for(std::chrono::seconds(1)), std::future_status::ready);
  EXPECT_TRUE(readRan.load());

  releaseTail.set_value();
  write.get();
}

TEST(DefaultWorkspaceLockTest, NewWriteWaitsForDowngradedWriteTail) {
  DefaultWorkspaceLock lock;

  std::promise<void> downgraded;
  auto downgradedFuture = downgraded.get_future();
  std::promise<void> releaseTail;
  auto releaseTailFuture = releaseTail.get_future().share();
  std::atomic<bool> tailFinished = false;
  std::atomic<bool> secondSawFinishedTail = false;

  auto first = lock.write([&](const utils::CancellationToken &,
                             const auto &downgrade) {
    downgrade();
    downgraded.set_value();
    releaseTailFuture.wait();
    tailFinished = true;
  });

  ASSERT_EQ(downgradedFuture.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);

  auto second = lock.write([&](const utils::CancellationToken &, const auto &) {
    secondSawFinishedTail = tailFinished.load();
  });

  // Give the second write a chance to (incorrectly) start before the first's
  // downgraded tail has finished; it must not, because the downgraded write
  // still counts as a reader.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  releaseTail.set_value();

  first.get();
  second.get();
  EXPECT_TRUE(secondSawFinishedTail.load());
}

TEST(DefaultWorkspaceLockTest, DestructorDrainsReadsBlockedOnAHeldWrite) {
  // read() runs synchronously in the caller's thread, so the blocking reads must
  // each run on their own thread for the main thread to reach the destructor.
  std::vector<std::thread> readThreads;
  std::atomic<int> readsCompleted = 0;
  {
    DefaultWorkspaceLock lock;

    std::promise<void> writeRunning;
    auto writeRunningFuture = writeRunning.get_future();
    auto write = lock.write([&](const utils::CancellationToken &token,
                                const auto &) {
      writeRunning.set_value();
      while (!token.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });
    ASSERT_EQ(writeRunningFuture.wait_for(std::chrono::seconds(1)),
              std::future_status::ready);

    // These reads block because the write holds exclusivity.
    readThreads.reserve(8);
    for (int index = 0; index < 8; ++index) {
      readThreads.emplace_back([&lock, &readsCompleted]() {
        lock.read([]() {}).get();
        ++readsCompleted;
      });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // The lock is destroyed here while reads are blocked: it must cancel the
    // write, drain the blocked reads, and not tear down the gate under them.
  }

  for (auto &thread : readThreads) {
    thread.join();
  }
  EXPECT_EQ(readsCompleted.load(), 8);
}

TEST(DefaultWorkspaceLockTest, CancelWriteStopsCurrentWriteAction) {
  DefaultWorkspaceLock lock;

  std::promise<void> started;
  auto startedFuture = started.get_future();
  std::atomic<bool> observedCancellation = false;

  auto future = lock.write([&](const utils::CancellationToken &token,
                               const auto &) {
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
