#pragma once

#include <pegium/services/DefaultLanguageService.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {

class DefaultRenameProvider : protected services::DefaultLanguageService,
                                public services::RenameProvider {
public:
  using services::DefaultLanguageService::DefaultLanguageService;

  std::optional<::lsp::WorkspaceEdit>
  rename(const workspace::Document &document,
         const ::lsp::RenameParams &params,
         const utils::CancellationToken &cancelToken) const override;

  std::optional<::lsp::PrepareRenameResult>
  prepareRename(const workspace::Document &document,
                const ::lsp::TextDocumentPositionParams &params,
                const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium::lsp
