#include <pegium/lsp/services/AbstractExecuteCommandHandler.hpp>

#include <utility>

namespace pegium {

std::vector<std::string> AbstractExecuteCommandHandler::commands() const {
  ensureInitialized();
  return _commandNames;
}

std::optional<::lsp::LSPAny>
AbstractExecuteCommandHandler::executeCommand(
    std::string_view name, const ::lsp::LSPArray &arguments,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  ensureInitialized();
  const auto it = _registeredCommands.find(name);
  if (it == _registeredCommands.end()) {
    return std::nullopt;
  }
  return it->second(arguments, cancelToken);
}

void AbstractExecuteCommandHandler::ensureInitialized() const {
  std::call_once(_initializationFlag, [this]() {
    registerCommands(createCommandAcceptor());
  });
}

ExecuteCommandAcceptor AbstractExecuteCommandHandler::createCommandAcceptor() const {
  return [this](const std::string &name, ExecuteCommandFunction execute) {
    auto [it, inserted] =
        _registeredCommands.insert_or_assign(name, std::move(execute));
    if (inserted) {
      _commandNames.push_back(it->first);
    }
  };
}

} // namespace pegium
