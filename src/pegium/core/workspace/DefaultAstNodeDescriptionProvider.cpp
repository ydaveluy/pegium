#include <pegium/core/workspace/DefaultAstNodeDescriptionProvider.hpp>

#include <cassert>
#include <typeindex>
#include <typeinfo>
#include <utility>

#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::workspace {

std::optional<AstNodeDescription>
DefaultAstNodeDescriptionProvider::createDescription(
    const AstNode &node, const Document &document,
    references::AstNodeName nameInfo) const {
  if (nameInfo.empty()) {
    return std::nullopt;
  }
  assert(document.id != InvalidDocumentId);
  assert(node.hasCstNode());

  // Prefer the name's CST source provided by the caller; fall back to the
  // node's own CST node when the name was assigned outside the parser.
  const auto cstNode =
      nameInfo.cstNode.valid() ? nameInfo.cstNode : node.getCstNode();
  const auto offset = cstNode.getBegin();
  const auto nameLength = cstNode.getEnd() - cstNode.getBegin();
  assert(nameLength > 0);

  const auto symbolId = document.makeSymbolId(node);

  return AstNodeDescription{
      .name = std::move(nameInfo.name),
      .type = std::type_index(typeid(node)),
      .documentId = document.id,
      .symbolId = symbolId,
      .offset = offset,
      .nameLength = nameLength,
  };
}

} // namespace pegium::workspace
