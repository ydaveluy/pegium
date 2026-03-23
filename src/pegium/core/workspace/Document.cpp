#include <pegium/core/workspace/Document.hpp>

#include <cassert>
#include <utility>

namespace pegium::workspace {
namespace {

std::string resolve_document_uri(
    const std::shared_ptr<TextDocument> &textDocument, std::string uri) {
  assert(textDocument != nullptr);
  return uri.empty() ? textDocument->uri() : std::move(uri);
}

} // namespace

Document::Document(std::shared_ptr<TextDocument> textDocument, std::string uri)
    : uri(resolve_document_uri(textDocument, std::move(uri))),
      _textDocument(std::move(textDocument)) {
  assert(_textDocument != nullptr);
}
Document::~Document() = default;

void Document::resetAnalysisState() noexcept {
  state = DocumentState::Changed;
  parseResult = {};
  localSymbols.clear();
  references.clear();
  diagnostics.clear();
  std::scoped_lock lock(_astNodeIndexMutex);
  _astNodesBySymbolId.clear();
  _astNodeIndexBuilt = false;
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
  assert(node.hasCstNode() && "AST nodes should always carry a CST node");
  return static_cast<SymbolId>(node.getCstNode().id());
}

const AstNode &Document::getAstNode(SymbolId symbolId) const noexcept {
  assert(symbolId != InvalidSymbolId);
  assert(hasAst());

  std::scoped_lock lock(_astNodeIndexMutex);
  if (!_astNodeIndexBuilt) {
    buildAstNodeIndexLocked();
  }

  const auto index = static_cast<std::size_t>(symbolId);
  assert(index < _astNodesBySymbolId.size());
  const auto *node = _astNodesBySymbolId[index];
  assert(node != nullptr);
  return *node;
}

const AstNode *Document::findAstNode(SymbolId symbolId) const noexcept {
  assert(symbolId != InvalidSymbolId);
  assert(hasAst());

  std::scoped_lock lock(_astNodeIndexMutex);
  if (!_astNodeIndexBuilt) {
    buildAstNodeIndexLocked();
  }

  const auto index = static_cast<std::size_t>(symbolId);
  return index < _astNodesBySymbolId.size() ? _astNodesBySymbolId[index]
                                            : nullptr;
}

void Document::buildAstNodeIndexLocked() const {
  assert(hasAst());
  _astNodesBySymbolId.clear();

  auto registerNode = [this](const AstNode &node) {
    if (!node.hasCstNode()) {
      return;
    }
    const auto symbolId = static_cast<std::size_t>(node.getCstNode().id());
    if (symbolId >= _astNodesBySymbolId.size()) {
      _astNodesBySymbolId.resize(symbolId + 1, nullptr);
    }
    _astNodesBySymbolId[symbolId] = &node;
  };

  const auto &root = *parseResult.value;
  registerNode(root);
  for (const auto *node : root.getAllContent()) {
    registerNode(*node);
  }

  _astNodeIndexBuilt = true;
}

} // namespace pegium::workspace
