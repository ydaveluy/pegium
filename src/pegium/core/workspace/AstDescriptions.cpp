#include <pegium/core/workspace/AstDescriptions.hpp>

#include <cassert>
#include <string>

#include <pegium/core/utils/Errors.hpp>
#include <pegium/core/workspace/Documents.hpp>

namespace pegium::workspace {

const AstNode &resolve_ast_node(const Documents &documents,
                                const AstNodeDescription &description) {
  assert(description.symbolId != InvalidSymbolId);

  const auto document = documents.getDocument(description.documentId);
  if (document == nullptr) {
    // The target document was rebuilt away or deleted without this dependent
    // being relinked (the build's relink invariant was violated). Fail
    // catchably instead of dereferencing null; the linker maps it to a
    // LinkingError. getAstNode below likewise guards a stale symbolId.
    throw utils::MissingAstDocumentError(
        "resolve_ast_node: target document " +
        std::to_string(description.documentId) +
        " no longer exists (stale reference?)");
  }
  return document->getAstNode(description.symbolId);
}

const AstNode &resolve_ast_node(const Documents &documents,
                                const AstNodeDescription &description,
                                const Document &currentDocument) {
  assert(description.documentId != InvalidDocumentId);
  assert(description.symbolId != InvalidSymbolId);

  if (currentDocument.id == description.documentId) {
    return currentDocument.getAstNode(description.symbolId);
  }

  return resolve_ast_node(documents, description);
}

} // namespace pegium::workspace
