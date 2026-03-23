#include <pegium/lsp/runtime/LanguageServerHandlerContext.hpp>

#include <utility>

#include <pegium/lsp/services/SharedServices.hpp>
#include <pegium/lsp/workspace/WorkspaceAdapters.hpp>

namespace pegium {

LanguageServerHandlerContext::LanguageServerHandlerContext(
    LanguageServer &languageServer, pegium::SharedServices &sharedServices,
    LanguageServerRuntimeState &runtimeState) noexcept
    : _languageServer(languageServer), _sharedServices(sharedServices),
      _runtimeState(runtimeState) {}

::lsp::InitializeResult
LanguageServerHandlerContext::initialize(const ::lsp::InitializeParams &params) {
  setInitializeCapabilities(to_workspace_initialize_params(params).capabilities);
  return _languageServer.initialize(params);
}

void LanguageServerHandlerContext::initialized(
    const ::lsp::InitializedParams &params) {
  _languageServer.initialized(params);
}

} // namespace pegium
