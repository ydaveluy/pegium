#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <pegium/core/utils/Collections.hpp>
#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/utils/Event.hpp>
#include <pegium/core/utils/Index.hpp>

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

TEST(UtilsTest, CollectionHelpersExposeContainsKeysAndValues) {
  const std::unordered_map<std::string, int> map{{"a", 1}, {"b", 2}};
  EXPECT_TRUE(pegium::utils::contains(map, std::string("a")));
  EXPECT_FALSE(pegium::utils::contains(map, std::string("z")));

  const std::vector<int> values{1, 2, 3};
  EXPECT_TRUE(pegium::utils::contains_linear(values, 2));
  EXPECT_FALSE(pegium::utils::contains_linear(values, 8));

  const auto mapKeys = pegium::utils::keys(map);
  const auto mapValues = pegium::utils::values(map);
  EXPECT_EQ(mapKeys.size(), map.size());
  EXPECT_EQ(mapValues.size(), map.size());
}

} // namespace
