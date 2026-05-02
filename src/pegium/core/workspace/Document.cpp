#include <pegium/core/workspace/Document.hpp>

#include <cassert>
#include <utility>

#include <pegium/core/syntax-tree/AstArena.hpp>

namespace pegium::workspace {
namespace {

std::string resolve_document_uri(
    const std::shared_ptr<TextDocument> &textDocument, const std::string &uri) {
  assert(textDocument != nullptr);
  return uri.empty() ? textDocument->uri() : uri;
}

} // namespace

Document::Document(std::shared_ptr<TextDocument> textDocument, std::string uri)
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

const AstNode &Document::getAstNode(SymbolId symbolId) const noexcept {
  assert(symbolId != InvalidSymbolId);
  assert(parseResult.astArena != nullptr);
  const auto *node = parseResult.astArena->getNode(symbolId);
  assert(node != nullptr);
  return *node;
}

const AstNode *Document::findAstNode(SymbolId symbolId) const noexcept {
  if (symbolId == InvalidSymbolId || parseResult.astArena == nullptr) {
    return nullptr;
  }
  return parseResult.astArena->getNode(symbolId);
}

} // namespace pegium::workspace
