#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <lsp/types.h>

#include <pegium/lsp/LanguageServer.hpp>
#include <pegium/lsp/LanguageServerRuntimeState.hpp>
#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/WorkspaceProtocol.hpp>

namespace pegium::services {
struct SharedServices;
}

namespace pegium::lsp {

class LanguageServerHandlerContext {
public:
  LanguageServerHandlerContext(LanguageServer &languageServer,
                               services::SharedServices &sharedServices,
                               LanguageServerRuntimeState &runtimeState) noexcept;

  [[nodiscard]] services::SharedServices &sharedServices() noexcept {
    return _sharedServices;
  }
  [[nodiscard]] const services::SharedServices &sharedServices() const noexcept {
    return _sharedServices;
  }

  [[nodiscard]] bool handlersRegistered() const noexcept {
    return _handlersRegistered;
  }
  void setHandlersRegistered(bool value) noexcept { _handlersRegistered = value; }

  [[nodiscard]] bool initialized() const noexcept {
    return _runtimeState.initialized();
  }
  void setInitialized(bool value) noexcept {
    _runtimeState.setInitialized(value);
  }

  [[nodiscard]] bool shutdownRequested() const noexcept {
    return _runtimeState.shutdownRequested();
  }
  void setShutdownRequested(bool value) noexcept {
    _runtimeState.setShutdownRequested(value);
  }

  [[nodiscard]] bool exitRequested() const noexcept {
    return _runtimeState.exitRequested();
  }
  void setExitRequested(bool value) noexcept {
    _runtimeState.setExitRequested(value);
  }

  [[nodiscard]] const workspace::InitializeCapabilities &
  initializeCapabilities() const noexcept {
    return _runtimeState.initializeCapabilities();
  }
  void setInitializeCapabilities(
      workspace::InitializeCapabilities capabilities) noexcept {
    _runtimeState.setInitializeCapabilities(std::move(capabilities));
  }

  [[nodiscard]] ::lsp::InitializeResult
  initialize(const ::lsp::InitializeParams &params);
  void initialized(const ::lsp::InitializedParams &params);

  void registerRequestCancellation(
      std::string requestKey,
      std::shared_ptr<utils::CancellationTokenSource> source) {
    _runtimeState.registerRequestCancellation(std::move(requestKey),
                                              std::move(source));
  }
  void clearRequestCancellation(
      std::string_view requestKey,
      const std::shared_ptr<utils::CancellationTokenSource> &source) {
    _runtimeState.clearRequestCancellation(requestKey, source);
  }
  void cancelRequestByKey(std::string_view requestKey) {
    _runtimeState.cancelRequestByKey(requestKey);
  }
  void waitForPendingRequests() { _runtimeState.waitForPendingRequests(); }

private:
  LanguageServer &_languageServer;
  services::SharedServices &_sharedServices;
  LanguageServerRuntimeState &_runtimeState;
  bool _handlersRegistered = false;
};

} // namespace pegium::lsp
