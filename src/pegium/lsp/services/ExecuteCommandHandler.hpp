#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>

namespace pegium {

/// One registered execute-command callback.
using ExecuteCommandFunction = std::function<
    std::optional<::lsp::LSPAny>(const ::lsp::LSPArray &arguments,
                                 const utils::CancellationToken &cancelToken)>;

/// Callback used to register a command name together with its handler.
using ExecuteCommandAcceptor =
    std::function<void(std::string name, ExecuteCommandFunction execute)>;

/// Handles `workspace/executeCommand` requests.
class ExecuteCommandHandler {
public:
  virtual ~ExecuteCommandHandler() noexcept = default;

  /// Returns the command names advertised by this handler.
  [[nodiscard]] virtual std::vector<std::string> commands() const = 0;

  /// Executes the named command with its JSON-like argument payload.
  [[nodiscard]] virtual std::optional<::lsp::LSPAny>
  executeCommand(std::string_view name, const ::lsp::LSPArray &arguments,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token) const = 0;
};

} // namespace pegium
