#pragma once

#include <optional>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class RenameProvider {
public:
  virtual ~RenameProvider() noexcept = default;

  virtual std::optional<::lsp::WorkspaceEdit>
  rename(const workspace::Document &document,
         const ::lsp::RenameParams &params,
         const utils::CancellationToken &cancelToken =
             utils::default_cancel_token) const {
    (void)document;
    (void)params;
    (void)cancelToken;
    return std::nullopt;
  }

  virtual std::optional<::lsp::PrepareRenameResult>
  prepareRename(const workspace::Document &document,
                const ::lsp::TextDocumentPositionParams &params,
                const utils::CancellationToken &cancelToken =
                    utils::default_cancel_token) const {
    (void)document;
    (void)params;
    (void)cancelToken;
    return std::nullopt;
  }
};

} // namespace pegium::services
