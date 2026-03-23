#include <pegium/core/workspace/DefaultAstNodeDescriptionProvider.hpp>

#include <cassert>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <utility>

#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::workspace {

std::optional<AstNodeDescription>
DefaultAstNodeDescriptionProvider::createDescription(const AstNode &node,
                                                     const Document &document,
                                                     std::string name) const {
  if (name.empty()) {
    return std::nullopt;
  }
  assert(document.id != InvalidDocumentId);
  assert(node.hasCstNode());

  const auto cstNode = services.references.nameProvider->getNameNode(node)
                           .value_or(node.getCstNode());
  const auto offset = cstNode.getBegin();
  const auto nameLength = cstNode.getEnd() - cstNode.getBegin();
  assert(nameLength > 0);

  const auto symbolId = document.makeSymbolId(node);

  return AstNodeDescription{
      .name = std::move(name),
      .type = std::type_index(typeid(node)),
      .documentId = document.id,
      .symbolId = symbolId,
      .offset = offset,
      .nameLength = nameLength,
  };
}

} // namespace pegium::workspace
