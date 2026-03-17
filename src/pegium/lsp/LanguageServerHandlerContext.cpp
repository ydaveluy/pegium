#include <pegium/lsp/LanguageServerHandlerContext.hpp>

#include <utility>

#include <pegium/services/SharedServices.hpp>
#include <pegium/lsp/WorkspaceAdapters.hpp>

namespace pegium::lsp {

LanguageServerHandlerContext::LanguageServerHandlerContext(
    LanguageServer &languageServer, services::SharedServices &sharedServices,
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

} // namespace pegium::lsp
