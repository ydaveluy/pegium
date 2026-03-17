#include <pegium/lsp/DefaultDefinitionProvider.hpp>
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

std::optional<std::vector<::lsp::LocationLink>>
DefaultDefinitionProvider::getDefinition(
    const workspace::Document &document, const ::lsp::DefinitionParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto *references = languageServices.references.references.get();
  if (references == nullptr) {
    return std::nullopt;
  }
  const auto offset = document.positionToOffset(params.position);
  const auto declaration = references->findDeclarationAt(document, offset);
  if (!declaration.has_value()) {
    return std::nullopt;
  }
  const auto location = to_location(*declaration);
  if (const auto locationLink =
          to_location_link(document, offset, location,
                           languageServices.sharedServices);
      locationLink.has_value()) {
    return std::vector<::lsp::LocationLink>{std::move(*locationLink)};
  }
  return std::nullopt;
}

} // namespace pegium::lsp
