#include <pegium/lsp/DefaultRenameProvider.hpp>
#include <pegium/lsp/LspProviderUtils.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <pegium/services/SharedServices.hpp>

namespace pegium::lsp {

using namespace detail;

std::optional<::lsp::WorkspaceEdit> DefaultRenameProvider::rename(
    const workspace::Document &document, const ::lsp::RenameParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto offset = document.positionToOffset(params.position);
  const auto newName = std::string_view(params.newName);
  const auto *referencesService = languageServices.references.references.get();
  if (referencesService == nullptr || newName.empty()) {
    return std::nullopt;
  }

  if (!referencesService->findDeclarationAt(document, offset).has_value()) {
    return std::nullopt;
  }

  WorkspaceEditData edit;
  std::unordered_set<std::string> seen;
  for (const auto &reference :
       referencesService->findReferencesAt(document, offset,
                                           /*includeDeclaration=*/true)) {
    utils::throw_if_cancelled(cancelToken);
    auto location =
        std::visit([](const auto &entry) { return to_location(entry); }, reference);
    if (!seen.insert(location_key(location)).second) {
      continue;
    }
    edit.changes[location.documentId].push_back(
        {.begin = location.begin,
         .end = location.end,
         .newText = std::string(newName)});
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
  return to_lsp_workspace_edit(edit, languageServices.sharedServices,
                               cancelToken);
}

std::optional<::lsp::PrepareRenameResult> DefaultRenameProvider::prepareRename(
    const workspace::Document &document,
    const ::lsp::TextDocumentPositionParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto *referencesService = languageServices.references.references.get();
  if (referencesService == nullptr) {
    return std::nullopt;
  }
  const auto offset = document.positionToOffset(params.position);
  const auto token = token_at(document.text(), offset);
  if (token.text.empty()) {
    return std::nullopt;
  }
  if (!referencesService->findDeclarationAt(document, offset).has_value()) {
    return std::nullopt;
  }

  ::lsp::Range range{};
  range.start = document.offsetToPosition(token.begin);
  range.end = document.offsetToPosition(token.end);
  ::lsp::PrepareRenameResult result{};
  result = std::move(range);
  return result;
}

} // namespace pegium::lsp
