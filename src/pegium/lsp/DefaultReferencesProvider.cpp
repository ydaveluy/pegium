#include <pegium/lsp/DefaultReferencesProvider.hpp>
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

namespace {

std::optional<::lsp::Location>
to_lsp_location(const LocationData &location,
                const services::SharedServices &sharedServices) {
  if (sharedServices.workspace.documents == nullptr) {
    return std::nullopt;
  }
  const auto document =
      sharedServices.workspace.documents->getDocument(location.documentId);
  if (document == nullptr || document->textDocument() == nullptr) {
    return std::nullopt;
  }
  const auto textDocument = document->textDocument();

  ::lsp::Location result{};
  result.uri = ::lsp::Uri::parse(document->uri);
  result.range.start = textDocument->offsetToPosition(location.begin);
  result.range.end = textDocument->offsetToPosition(location.end);
  return result;
}

} // namespace

std::vector<::lsp::Location> DefaultReferencesProvider::findReferences(
    const workspace::Document &document, const ::lsp::ReferenceParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto offset = document.positionToOffset(params.position);
  const auto includeDeclaration = params.context.includeDeclaration;
  const auto *referencesService = languageServices.references.references.get();
  if (referencesService == nullptr) {
    return {};
  }

  std::vector<::lsp::Location> locations;
  std::unordered_set<std::string> seen;

  for (const auto &reference :
       referencesService->findReferencesAt(document, offset,
                                           includeDeclaration)) {
    utils::throw_if_cancelled(cancelToken);
    auto location =
        std::visit([](const auto &entry) { return to_location(entry); }, reference);
    if (!seen.insert(location_key(location)).second) {
      continue;
    }

    if (auto lspLocation =
            to_lsp_location(location, languageServices.sharedServices);
        lspLocation.has_value()) {
      locations.push_back(std::move(*lspLocation));
    }
  }
  return locations;
}

} // namespace pegium::lsp
