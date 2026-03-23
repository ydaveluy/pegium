#pragma once

#include <functional>

#include <lsp/types.h>

#include <pegium/core/utils/Disposable.hpp>


namespace pegium {

/// Minimal language-server lifecycle interface used by the runtime.
class LanguageServer {
public:
  virtual ~LanguageServer() noexcept = default;

  /// Handles the initialize request and returns the advertised capabilities.
  [[nodiscard]] virtual ::lsp::InitializeResult
  initialize(const ::lsp::InitializeParams &params) = 0;
  /// Handles the initialized notification.
  virtual void initialized(const ::lsp::InitializedParams &params) = 0;
  /// Subscribes to initialize requests.
  virtual utils::ScopedDisposable onInitialize(
      std::function<void(const ::lsp::InitializeParams &)> callback) = 0;
  /// Subscribes to initialized notifications.
  virtual utils::ScopedDisposable onInitialized(
      std::function<void(const ::lsp::InitializedParams &)> callback) = 0;
};

} // namespace pegium
