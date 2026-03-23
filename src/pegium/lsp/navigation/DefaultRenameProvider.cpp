#include <pegium/lsp/navigation/DefaultRenameProvider.hpp>
#include <pegium/lsp/support/LspProviderUtils.hpp>
#include <pegium/core/syntax-tree/CstUtils.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <pegium/lsp/services/SharedServices.hpp>
#include <pegium/core/utils/TransparentStringHash.hpp>

namespace pegium {
using namespace pegium::provider_detail;



std::optional<::lsp::WorkspaceEdit> DefaultRenameProvider::rename(
    const workspace::Document &document, const ::lsp::RenameParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto offset = document.textDocument().offsetAt(params.position);
  const auto newName = std::string_view(params.newName);
  const auto &referencesService = *services.references.references;
  if (newName.empty()) {
    return std::nullopt;
  }
  const auto declarations =
      find_declarations_at_offset(document, offset, referencesService);
  if (declarations.empty()) {
    return std::nullopt;
  }

  WorkspaceEditData edit;
  utils::TransparentStringSet seen;
  for (const auto *target : declarations) {
    for (const auto &reference : referencesService.findReferences(
             *target, {.includeDeclaration = true})) {
      utils::throw_if_cancelled(cancelToken);
      const auto location = to_location(reference);
      if (!seen.insert(location_key(location)).second) {
        continue;
      }
      edit.changes[location.documentId].push_back(
          {.begin = location.begin,
           .end = location.end,
           .newText = std::string(newName)});
    }
  }

  for (auto &[documentId, edits] : edit.changes) {
    utils::throw_if_cancelled(cancelToken);
    (void)documentId;
    std::ranges::sort(edits, [](const WorkspaceTextEdit &left,
                                const WorkspaceTextEdit &right) {
      return left.begin > right.begin;
    });
  }

  if (edit.empty()) {
    return std::nullopt;
  }
  return to_lsp_workspace_edit(edit, services.shared,
                               cancelToken);
}

std::optional<::lsp::PrepareRenameResult> DefaultRenameProvider::prepareRename(
    const workspace::Document &document,
    const ::lsp::TextDocumentPositionParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto &referencesService = *services.references.references;
  const auto &textDocument = document.textDocument();
  const auto offset = textDocument.offsetAt(params.position);
  const auto token = token_at(textDocument.getText(), offset);
  if (token.text.empty()) {
    return std::nullopt;
  }
  if (find_declarations_at_offset(document, offset, referencesService).empty()) {
    return std::nullopt;
  }

  ::lsp::Range range{};
  range.start = textDocument.positionAt(token.begin);
  range.end = textDocument.positionAt(token.end);
  ::lsp::PrepareRenameResult result{};
  result = std::move(range);
  return result;
}

} // namespace pegium
