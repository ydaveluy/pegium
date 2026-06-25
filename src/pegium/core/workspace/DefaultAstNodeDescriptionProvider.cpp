#include <pegium/core/workspace/DefaultAstNodeDescriptionProvider.hpp>

#include <cassert>
#include <typeindex>
#include <typeinfo>
#include <utility>

#include <pegium/core/workspace/Document.hpp>

namespace pegium::workspace {

std::optional<AstNodeDescription>
DefaultAstNodeDescriptionProvider::createDescription(
    const AstNode &node, std::string name, const Document &document) const {
  if (name.empty()) {
    return std::nullopt;
  }
  assert(document.id != InvalidDocumentId);

  return AstNodeDescription{
      .name = std::move(name),
      .type = std::type_index(typeid(node)),
      .documentId = document.id,
      .symbolId = document.makeSymbolId(node),
  };
}

} // namespace pegium::workspace
