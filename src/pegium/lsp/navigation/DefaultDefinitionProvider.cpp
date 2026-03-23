#include <pegium/lsp/navigation/DefaultDefinitionProvider.hpp>
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

namespace pegium {
using namespace pegium::provider_detail;



std::optional<std::vector<::lsp::LocationLink>>
DefaultDefinitionProvider::getDefinition(
    const workspace::Document &document, const ::lsp::DefinitionParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto &references = *services.references.references;
  const auto offset = document.textDocument().offsetAt(params.position);

  std::vector<::lsp::LocationLink> links;
  for (const auto &targetNode :
       find_declaration_nodes_at_offset(document, offset, references)) {
    const auto &targetDocument = targetNode.root().getDocument();
    links.push_back(to_location_link(document, offset,
                                     {.documentId = targetDocument.id,
                                      .begin = targetNode.getBegin(),
                                      .end = targetNode.getEnd()},
                                     services.shared));
  }

  if (links.empty()) {
    return std::nullopt;
  }
  return links;
}

} // namespace pegium
