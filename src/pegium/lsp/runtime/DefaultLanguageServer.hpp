#pragma once

#include <lsp/types.h>

#include <pegium/lsp/runtime/LanguageServer.hpp>
#include <pegium/lsp/services/DefaultSharedLspService.hpp>
#include <pegium/core/utils/Event.hpp>

namespace pegium {

/// Default language-server facade that stores initialize lifecycle subscribers.
class DefaultLanguageServer : public LanguageServer,
                              protected DefaultSharedLspService {
public:
  using DefaultSharedLspService::DefaultSharedLspService;
  ~DefaultLanguageServer() noexcept override = default;

  /// Processes the initialize request and returns the advertised capabilities.
  [[nodiscard]] ::lsp::InitializeResult
  initialize(const ::lsp::InitializeParams &params) override;
  /// Processes the initialized notification.
  void initialized(const ::lsp::InitializedParams &params) override;
  /// Subscribes to initialize requests.
  utils::ScopedDisposable
  onInitialize(std::function<void(const ::lsp::InitializeParams &)> callback)
      override;
  /// Subscribes to initialized notifications.
  utils::ScopedDisposable
  onInitialized(std::function<void(const ::lsp::InitializedParams &)> callback)
      override;

private:
  utils::EventEmitter<::lsp::InitializeParams> _onInitialize;
  utils::EventEmitter<::lsp::InitializedParams> _onInitialized;
  bool _initializeEventFired = false;
  bool _initializedEventFired = false;
};

} // namespace pegium
