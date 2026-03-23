#pragma once

#include <future>
#include <vector>

#include <lsp/types.h>

#include <pegium/core/services/JsonValue.hpp>

namespace pegium {

/// Bridge used by the server runtime to call back into the language client.
class LanguageClient {
public:
  virtual ~LanguageClient() noexcept = default;

  /// Sends a capability registration request to the client.
  [[nodiscard]] virtual std::future<void>
  registerCapability(::lsp::RegistrationParams params) = 0;

  /// Requests configuration sections from the client.
  [[nodiscard]] virtual std::future<std::vector<services::JsonValue>>
  fetchConfiguration(::lsp::ConfigurationParams params) = 0;
};

} // namespace pegium
