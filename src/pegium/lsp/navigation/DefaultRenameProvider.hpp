#pragma once

#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {

/// Default rename provider that validates and rewrites indexed references.
class DefaultRenameProvider : protected DefaultLanguageService,
                                public ::pegium::RenameProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

  std::optional<::lsp::WorkspaceEdit>
  rename(const workspace::Document &document,
         const ::lsp::RenameParams &params,
         const utils::CancellationToken &cancelToken) const override;

  std::optional<::lsp::PrepareRenameResult>
  prepareRename(const workspace::Document &document,
                const ::lsp::TextDocumentPositionParams &params,
                const utils::CancellationToken &cancelToken) const override;
};

} // namespace pegium
