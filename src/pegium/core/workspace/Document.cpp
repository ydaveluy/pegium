#include <pegium/core/workspace/Document.hpp>

#include <cassert>
#include <string>
#include <utility>

#include <pegium/core/syntax-tree/AstArena.hpp>
#include <pegium/core/utils/Errors.hpp>

namespace pegium::workspace {
namespace {

std::string resolve_document_uri(
    const std::shared_ptr<TextDocument> &textDocument, const std::string &uri) {
  assert(textDocument != nullptr);
  return uri.empty() ? textDocument->uri() : uri;
}

} // namespace

Document::Document(std::shared_ptr<TextDocument> textDocument,
                   const std::string &uri)
    : uri(resolve_document_uri(textDocument, uri)),
      _textDocument(std::move(textDocument)) {
  assert(_textDocument != nullptr);
}
Document::~Document() = default;

void Document::resetAnalysisState() noexcept {
  state = DocumentState::Changed;
  parseResult = {};
  localSymbols.clear();
  diagnostics.clear();
}

const TextDocument &Document::textDocument() const noexcept {
  return *_textDocument;
}

void Document::attachTextDocument(std::shared_ptr<TextDocument> textDocument) {
  assert(textDocument != nullptr);
  assert(textDocument->uri().empty() || textDocument->uri() == uri);

  _textDocument = std::move(textDocument);
}

SymbolId Document::makeSymbolId(const AstNode &node) const noexcept {
  return static_cast<SymbolId>(node.symbolId());
}

const AstNode &Document::getAstNode(SymbolId symbolId) const {
  assert(symbolId != InvalidSymbolId);
  const auto *node = parseResult.astArena != nullptr
                         ? parseResult.astArena->getNode(symbolId)
                         : nullptr;
  if (node == nullptr) {
    // The build relinks every dependent of a changed/deleted document
    // (DefaultDocumentBuilder::shouldRelink -> IndexManager::isAffected), so a
    // resolved description always points at a live node. If that invariant is
    // ever violated, throw a catchable error rather than dereferencing null (a
    // release-mode UAF); the linker turns it into a LinkingError.
    throw utils::MissingAstDocumentError(
        "Document::getAstNode: symbol " + std::to_string(symbolId) +
        " no longer resolves to a live AST node (stale reference?)");
  }
  return *node;
}

const AstNode *Document::findAstNode(SymbolId symbolId) const noexcept {
  if (symbolId == InvalidSymbolId || parseResult.astArena == nullptr) {
    return nullptr;
  }
  return parseResult.astArena->getNode(symbolId);
}

} // namespace pegium::workspace
