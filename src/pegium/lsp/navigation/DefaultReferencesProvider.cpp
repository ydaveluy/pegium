#include <pegium/lsp/navigation/DefaultReferencesProvider.hpp>
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

std::vector<::lsp::Location> DefaultReferencesProvider::findReferences(
    const workspace::Document &document, const ::lsp::ReferenceParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto offset = document.textDocument().offsetAt(params.position);
  const auto includeDeclaration = params.context.includeDeclaration;
  const auto &referencesService = *services.references.references;

  std::vector<::lsp::Location> locations;
  utils::TransparentStringSet seen;

  for (const auto *target :
       find_declarations_at_offset(document, offset, referencesService)) {
    for (const auto &reference : referencesService.findReferences(
             *target, {.includeDeclaration = includeDeclaration})) {
      utils::throw_if_cancelled(cancelToken);
      const auto location = to_location(reference);
      if (!seen.insert(location_key(location)).second) {
        continue;
      }
      const auto targetDocument =
          services.shared.workspace.documents->getDocument(location.documentId);
      assert(targetDocument != nullptr);
      const auto textDocument = targetDocument->textDocument();

      ::lsp::Location result{};
      result.uri = ::lsp::Uri::parse(targetDocument->uri);
      result.range.start = textDocument.positionAt(location.begin);
      result.range.end = textDocument.positionAt(location.end);
      locations.push_back(std::move(result));
    }
  }
  return locations;
}

} // namespace pegium
