#include <pegium/lsp/navigation/DefaultDefinitionProvider.hpp>
#include <pegium/lsp/support/LspProviderUtils.hpp>
#include <pegium/core/syntax-tree/AbstractReference.hpp>
#include <pegium/core/syntax-tree/CstUtils.hpp>

#include <optional>
#include <vector>

#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium {
using namespace pegium::provider_detail;

std::optional<std::vector<::lsp::LocationLink>>
DefaultDefinitionProvider::getDefinition(
    const workspace::Document &document, const ::lsp::DefinitionParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto &references = *services.references.references;
  const auto *nameProvider = services.references.nameProvider.get();
  const auto offset = document.textDocument().offsetAt(params.position);

  if (document.parseResult.cst == nullptr) {
    return std::nullopt;
  }
  const auto sourceNode =
      find_declaration_node_at_offset(*document.parseResult.cst, offset);
  if (!sourceNode.has_value()) {
    return std::nullopt;
  }

  // A cross-reference node spans the whole qualified name, so it is the source
  // selection; otherwise highlight the declaration node under the cursor.
  const auto *reference = find_reference_at_offset(document, offset);
  const auto originNode =
      reference != nullptr ? reference->getRefNode() : *sourceNode;

  std::vector<::lsp::LocationLink> links;
  for (const auto *targetDeclaration : references.findDeclarations(*sourceNode)) {
    links.push_back(to_location_link(document, originNode, *targetDeclaration,
                                     *nameProvider));
  }

  if (links.empty()) {
    return std::nullopt;
  }
  return links;
}

} // namespace pegium
