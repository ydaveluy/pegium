#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/WorkspaceProtocol.hpp>

namespace pegium::lsp {

class LanguageServerRuntimeState {
public:
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
  std::unordered_map<std::string, std::shared_ptr<utils::CancellationTokenSource>>
      _requestCancellation;
};

} // namespace pegium::lsp
