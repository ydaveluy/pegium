#pragma once

#include <mutex>
#include <vector>
#include <unordered_map>

#include <pegium/lsp/ExecuteCommandHandler.hpp>
#include <pegium/services/DefaultSharedLspService.hpp>
#include <pegium/utils/TransparentStringHash.hpp>

namespace pegium::lsp {

class AbstractExecuteCommandHandler : public ExecuteCommandHandler,
                                     protected services::DefaultSharedLspService {
public:
  using services::DefaultSharedLspService::DefaultSharedLspService;

  [[nodiscard]] std::vector<std::string> commands() const override;

  [[nodiscard]] std::optional<::lsp::LSPAny>
  executeCommand(std::string_view name, const ::lsp::LSPArray &arguments,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token) const override;

protected:
  virtual void registerCommands(const ExecuteCommandAcceptor &acceptor) = 0;

private:
  void ensureInitialized() const;
  [[nodiscard]] ExecuteCommandAcceptor createCommandAcceptor();

  mutable std::once_flag _initializationFlag;
  mutable std::vector<std::string> _commandNames;
  mutable utils::TransparentStringMap<ExecuteCommandFunction> _registeredCommands;
};

} // namespace pegium::lsp
