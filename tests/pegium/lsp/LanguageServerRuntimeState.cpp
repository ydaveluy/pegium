#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <memory>

#include <pegium/lsp/LanguageServerRuntimeState.hpp>

namespace pegium::lsp {
namespace {

TEST(LanguageServerRuntimeStateTest, TracksLifecycleFlags) {
  LanguageServerRuntimeState state;
  EXPECT_FALSE(state.initialized());
  EXPECT_FALSE(state.shutdownRequested());
  EXPECT_FALSE(state.exitRequested());

  state.setInitialized(true);
  state.setShutdownRequested(true);
  state.setExitRequested(true);

  EXPECT_TRUE(state.initialized());
  EXPECT_TRUE(state.shutdownRequested());
  EXPECT_TRUE(state.exitRequested());
}

TEST(LanguageServerRuntimeStateTest, ResetCancelsPendingRequestsAndClearsFlags) {
  LanguageServerRuntimeState state;
  auto source = std::make_shared<utils::CancellationTokenSource>();

  state.setInitialized(true);
  state.setShutdownRequested(true);
  state.setExitRequested(true);
  state.registerRequestCancellation("request-1", source);

  state.reset();

  EXPECT_FALSE(state.initialized());
  EXPECT_FALSE(state.shutdownRequested());
  EXPECT_FALSE(state.exitRequested());
  EXPECT_TRUE(source->stop_requested());
}

TEST(LanguageServerRuntimeStateTest,
     WaitForPendingRequestsReturnsAfterMatchingCancellationIsCleared) {
  LanguageServerRuntimeState state;
  auto source = std::make_shared<utils::CancellationTokenSource>();

  state.registerRequestCancellation("request-1", source);

  auto waiter = std::async(std::launch::async, [&state]() {
    state.waitForPendingRequests();
    return true;
  });

  EXPECT_EQ(waiter.wait_for(std::chrono::milliseconds(20)),
            std::future_status::timeout);

  state.clearRequestCancellation(
      "request-1", std::make_shared<utils::CancellationTokenSource>());
  EXPECT_EQ(waiter.wait_for(std::chrono::milliseconds(20)),
            std::future_status::timeout);

  state.clearRequestCancellation("request-1", source);
  EXPECT_EQ(waiter.wait_for(std::chrono::milliseconds(200)),
            std::future_status::ready);
  EXPECT_TRUE(waiter.get());
}

TEST(LanguageServerRuntimeStateTest, CancelRequestByKeyRequestsStop) {
  LanguageServerRuntimeState state;
  auto source = std::make_shared<utils::CancellationTokenSource>();
  state.registerRequestCancellation("request-1", source);

  state.cancelRequestByKey("request-1");

  EXPECT_TRUE(source->stop_requested());
}

} // namespace
} // namespace pegium::lsp
