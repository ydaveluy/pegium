#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/utils/TransparentStringHash.hpp>
#include <pegium/core/workspace/WorkspaceProtocol.hpp>

namespace pegium {

/// Mutable runtime state shared across LSP request handlers.
class LanguageServerRuntimeState {
public:
  /// Resets all lifecycle and pending-request state.
  void reset() noexcept;

  [[nodiscard]] bool initialized() const noexcept;
  void setInitialized(bool value) noexcept;

  [[nodiscard]] bool shutdownRequested() const noexcept;
  void setShutdownRequested(bool value) noexcept;

  [[nodiscard]] bool exitRequested() const noexcept;
  void setExitRequested(bool value) noexcept;

  [[nodiscard]] const workspace::InitializeCapabilities &
  initializeCapabilities() const noexcept;
  void setInitializeCapabilities(
      workspace::InitializeCapabilities capabilities) noexcept;

  void registerRequestCancellation(
      std::string requestKey,
      std::shared_ptr<utils::CancellationTokenSource> source);
  void clearRequestCancellation(
      std::string_view requestKey,
      const std::shared_ptr<utils::CancellationTokenSource> &source);
  void cancelRequestByKey(std::string_view requestKey);
  void waitForPendingRequests();

private:
  bool _initialized = false;
  bool _shutdownRequested = false;
  bool _exitRequested = false;
  workspace::InitializeCapabilities _initializeCapabilities;
  std::mutex _requestCancellationMutex;
  std::condition_variable _requestCancellationCv;
  utils::TransparentStringMap<std::shared_ptr<utils::CancellationTokenSource>>
      _requestCancellation;
};

} // namespace pegium
