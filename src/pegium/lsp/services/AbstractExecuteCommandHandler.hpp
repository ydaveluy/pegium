#pragma once

#include <mutex>
#include <vector>
#include <unordered_map>

#include <pegium/lsp/services/ExecuteCommandHandler.hpp>
#include <pegium/lsp/services/DefaultSharedLspService.hpp>
#include <pegium/core/utils/TransparentStringHash.hpp>

namespace pegium {

/// Base command handler that lazily registers named execute-command callbacks.
class AbstractExecuteCommandHandler : public ExecuteCommandHandler,
                                     protected DefaultSharedLspService {
public:
  using DefaultSharedLspService::DefaultSharedLspService;

  [[nodiscard]] std::vector<std::string> commands() const override;

  [[nodiscard]] std::optional<::lsp::LSPAny>
  executeCommand(std::string_view name, const ::lsp::LSPArray &arguments,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token) const override;

protected:
  /// Registers the commands exposed by this handler.
  virtual void registerCommands(const ExecuteCommandAcceptor &acceptor) const = 0;

private:
  void ensureInitialized() const;
  [[nodiscard]] ExecuteCommandAcceptor createCommandAcceptor() const;

  mutable std::once_flag _initializationFlag;
  mutable std::vector<std::string> _commandNames;
  mutable utils::TransparentStringMap<ExecuteCommandFunction> _registeredCommands;
};

} // namespace pegium
