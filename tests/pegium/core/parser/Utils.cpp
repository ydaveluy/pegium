#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/utils/Disposable.hpp>
#include <pegium/core/utils/Errors.hpp>
#include <pegium/core/utils/Event.hpp>

namespace {

TEST(UtilsTest, CancellationAndErrorsWork) {
  pegium::utils::CancellationTokenSource source;
  const auto token = source.get_token();
  EXPECT_FALSE(token.stop_requested());

  source.request_stop();
  EXPECT_TRUE(token.stop_requested());
  EXPECT_THROW(pegium::utils::throw_if_cancelled(token),
               pegium::utils::OperationCancelled);

  source = pegium::utils::CancellationTokenSource{};
  EXPECT_FALSE(source.get_token().stop_requested());
}

TEST(UtilsTest, DisposableAndEventEmitterSupportSubscriptionLifecycle) {
  pegium::utils::EventEmitter<int> emitter;
  int sum = 0;
  auto subscription = emitter.on([&sum](const int &value) { sum += value; });
  EXPECT_EQ(emitter.listenerCount(), 1u);

  emitter.emit(3);
  EXPECT_EQ(sum, 3);

  subscription.dispose();
  emitter.emit(5);
  EXPECT_EQ(sum, 3);
  EXPECT_EQ(emitter.listenerCount(), 0u);

  int disposed = 0;
  pegium::utils::DisposableStore store;
  store.add(pegium::utils::ScopedDisposable([&disposed]() { ++disposed; }));
  store.add(pegium::utils::ScopedDisposable([&disposed]() { ++disposed; }));
  store.dispose();
  EXPECT_EQ(disposed, 2);
  EXPECT_TRUE(store.disposed());
}

TEST(UtilsTest, EventEmitterPreservesRegistrationOrder) {
  pegium::utils::EventEmitter<int> emitter;
  std::vector<int> calls;

  auto first = emitter.on([&calls](const int &value) { calls.push_back(value); });
  auto second =
      emitter.on([&calls](const int &value) { calls.push_back(value * 10); });
  auto third =
      emitter.on([&calls](const int &value) { calls.push_back(value * 100); });

  emitter.emit(2);
  EXPECT_EQ(calls, (std::vector<int>{2, 20, 200}));

  calls.clear();
  second.dispose();
  emitter.emit(3);
  EXPECT_EQ(calls, (std::vector<int>{3, 300}));

  first.dispose();
  third.dispose();
}

TEST(UtilsTest, EventEmitterSupportsConcurrentSubscriptionAndEmit) {
  pegium::utils::EventEmitter<int> emitter;
  std::atomic<int> calls = 0;
  std::atomic<bool> start = false;

  constexpr std::size_t kWorkerCount = 4;
  constexpr std::size_t kIterations = 64;
  std::vector<std::jthread> workers;
  workers.reserve(kWorkerCount);

  for (std::size_t worker = 0; worker < kWorkerCount; ++worker) {
    workers.emplace_back([&]() {
      while (!start.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      for (std::size_t iteration = 0; iteration < kIterations; ++iteration) {
        auto subscription = emitter.on([&calls](const int &value) {
          calls.fetch_add(value, std::memory_order_relaxed);
        });
        emitter.emit(1);
        if (iteration % 2 == 0) {
          subscription.dispose();
        }
      }
    });
  }

  start.store(true, std::memory_order_release);
  workers.clear();

  EXPECT_GT(calls.load(std::memory_order_relaxed), 0);
}

} // namespace
