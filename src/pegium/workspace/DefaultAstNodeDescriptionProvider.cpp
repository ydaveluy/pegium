#include <pegium/workspace/DefaultAstNodeDescriptionProvider.hpp>

#include <typeindex>
#include <typeinfo>
#include <utility>

#include <pegium/services/CoreServices.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::workspace {

std::optional<AstNodeDescription>
DefaultAstNodeDescriptionProvider::createDescription(const AstNode &node,
                                                     const Document &document,
                                                     std::string name) const {
  const auto *nameProvider = coreServices.references.nameProvider.get();
  if (nameProvider == nullptr) {
    return std::nullopt;
  }

  if (name.empty()) {
    name = nameProvider->getName(node);
  }
  if (name.empty()) {
    return std::nullopt;
  }

  TextOffset offset = 0;
  TextOffset nameLength = 0;
  const auto nameNode = nameProvider->getNameNode(node);
  if (nameNode.valid()) {
    offset = nameNode.getBegin();
    nameLength =
        static_cast<TextOffset>(nameNode.getEnd() - nameNode.getBegin());
  }

  return AstNodeDescription{
      .name = std::move(name),
      .node = &node,
      .type = std::type_index(typeid(node)),
      .documentId = document.id,
      .symbolId = document.makeSymbolId(node),
      .offset = offset,
      .nameLength = nameLength,
  };
}

} // namespace pegium::workspace
