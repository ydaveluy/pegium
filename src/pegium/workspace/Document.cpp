#include <pegium/workspace/Document.hpp>

#include <cassert>
#include <memory>
#include <utility>

namespace pegium::workspace {

namespace {

std::shared_ptr<const TextDocument> make_empty_text_document() {
  return std::make_shared<TextDocument>();
}

} // namespace

Document::Document() : _textDocument(make_empty_text_document()) {}
Document::~Document() = default;

void Document::resetAnalysisState() noexcept {
  state = DocumentState::Changed;
  parseResult = {};
  localSymbols.clear();
  references.clear();
  referenceDescriptions.clear();
  diagnostics.clear();
  std::scoped_lock lock(_astNodeIndexMutex);
  _astNodesBySymbolId.clear();
  _astNodeIndexBuilt = false;
}

std::shared_ptr<const TextDocument> Document::textDocument() const noexcept {
  return _textDocument;
}

void Document::setTextDocument(std::shared_ptr<const TextDocument> textDocument) {
  if (textDocument == nullptr) {
    textDocument = make_empty_text_document();
  }

  const auto textChanged = _textDocument == nullptr ||
                           _textDocument->text() != textDocument->text() ||
                           _textDocument->languageId != textDocument->languageId;
  if (textChanged) {
    resetAnalysisState();
  }

  _textDocument = std::move(textDocument);
  if (!_textDocument->uri.empty()) {
    uri = _textDocument->uri;
  }
  if (!_textDocument->languageId.empty()) {
    languageId = _textDocument->languageId;
  }
}

const std::string &Document::text() const noexcept {
  static const std::string empty;
  return _textDocument == nullptr ? empty : _textDocument->text();
}

std::string_view Document::textView() const noexcept {
  return _textDocument == nullptr ? std::string_view{} : _textDocument->textView();
}

std::uint64_t Document::revision() const noexcept {
  return _textDocument == nullptr ? 0u : _textDocument->revision();
}

std::optional<std::int64_t> Document::clientVersion() const noexcept {
  return _textDocument == nullptr ? std::nullopt : _textDocument->clientVersion();
}

void Document::setClientVersion(std::optional<std::int64_t> version) noexcept {
  if (_textDocument == nullptr) {
    auto textDocument = std::make_shared<TextDocument>();
    textDocument->setClientVersion(version);
    _textDocument = std::move(textDocument);
    return;
  }
  auto textDocument = std::make_shared<TextDocument>(*_textDocument);
  textDocument->setClientVersion(version);
  _textDocument = std::move(textDocument);
}

void Document::replaceText(std::string newText) {
  auto textDocument = _textDocument == nullptr
                          ? std::make_shared<TextDocument>()
                          : std::make_shared<TextDocument>(*_textDocument);
  textDocument->replaceText(std::move(newText));
  setTextDocument(std::move(textDocument));
}

void Document::applyContentChanges(std::span<const DocumentContentChange> changes) {
  if (changes.empty()) {
    return;
  }
  auto textDocument = _textDocument == nullptr
                          ? std::make_shared<TextDocument>()
                          : std::make_shared<TextDocument>(*_textDocument);
  textDocument->applyContentChanges(changes);
  setTextDocument(std::move(textDocument));
}

void Document::setText(std::string newText) { replaceText(std::move(newText)); }

TextOffset Document::positionToOffset(const text::Position &position) const {
  return _textDocument == nullptr ? 0u : _textDocument->positionToOffset(position);
}

TextOffset Document::positionToOffset(std::uint32_t line,
                                      std::uint32_t character) const {
  return _textDocument == nullptr ? 0u
                                  : _textDocument->positionToOffset(line, character);
}

text::Position Document::offsetToPosition(TextOffset offset) const {
  return _textDocument == nullptr ? text::Position{}
                                  : _textDocument->offsetToPosition(offset);
}

SymbolId Document::makeSymbolId(const AstNode &node) const noexcept {
  assert(node.hasCstNode() && "AST nodes should always carry a CST node");
  return node.hasCstNode() ? static_cast<SymbolId>(node.getCstNode().id())
                           : InvalidSymbolId;
}

const AstNode *Document::findAstNode(SymbolId symbolId) const noexcept {
  if (symbolId == InvalidSymbolId || !hasAst() || parseResult.value == nullptr) {
    return nullptr;
  }

  std::scoped_lock lock(_astNodeIndexMutex);
  if (!_astNodeIndexBuilt) {
    buildAstNodeIndexLocked();
  }

  const auto index = static_cast<std::size_t>(symbolId);
  return index < _astNodesBySymbolId.size() ? _astNodesBySymbolId[index]
                                            : nullptr;
}

void Document::buildAstNodeIndexLocked() const {
  _astNodesBySymbolId.clear();

  auto registerNode = [this](const AstNode *node) {
    if (node == nullptr || !node->hasCstNode()) {
      return;
    }
    const auto symbolId = static_cast<std::size_t>(node->getCstNode().id());
    if (symbolId >= _astNodesBySymbolId.size()) {
      _astNodesBySymbolId.resize(symbolId + 1, nullptr);
    }
    _astNodesBySymbolId[symbolId] = node;
  };

  const auto *root = parseResult.value.get();
  registerNode(root);
  if (root != nullptr) {
    for (const auto *node : root->getAllContent()) {
      registerNode(node);
    }
  }

  _astNodeIndexBuilt = true;
}

} // namespace pegium::workspace
