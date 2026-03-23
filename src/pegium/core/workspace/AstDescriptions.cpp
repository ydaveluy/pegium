#include <pegium/core/workspace/AstDescriptions.hpp>

#include <cassert>
#include <pegium/core/workspace/Documents.hpp>

namespace pegium::workspace {

const AstNode &resolve_ast_node(const Documents &documents,
                                const AstNodeDescription &description) noexcept {
  assert(description.documentId != InvalidDocumentId);
  assert(description.symbolId != InvalidSymbolId);

  const auto document = documents.getDocument(description.documentId);
  assert(document != nullptr);
  return document->getAstNode(description.symbolId);
}

const AstNode &resolve_ast_node(const Documents &documents,
                                const AstNodeDescription &description,
                                const Document &currentDocument) noexcept {
  assert(description.documentId != InvalidDocumentId);
  assert(description.symbolId != InvalidSymbolId);

  if (currentDocument.id == description.documentId) {
    return currentDocument.getAstNode(description.symbolId);
  }

  return resolve_ast_node(documents, description);
}

} // namespace pegium::workspace
