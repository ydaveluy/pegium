#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <pegium/parser/Parser.hpp>
#include <pegium/services/Diagnostic.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/syntax-tree/RootCstNode.hpp>
#include <pegium/syntax-tree/Reference.hpp>
#include <pegium/text/Position.hpp>
#include <pegium/workspace/LocalSymbols.hpp>
#include <pegium/workspace/Symbol.hpp>
#include <pegium/workspace/TextDocument.hpp>

namespace pegium::workspace {

enum class DocumentState : std::uint8_t {
  Changed,
  Parsed,
  IndexedContent,
  ComputedScopes,
  Linked,
  IndexedReferences,
  Validated,
};

using DocumentContentChangeRange = TextDocumentContentChangeRange;
using DocumentContentChange = TextDocumentContentChange;

struct Document {
  DocumentId id = InvalidDocumentId;
  std::string uri;
  std::string languageId;

  DocumentState state = DocumentState::Changed;

  parser::ParseResult parseResult;
  LocalSymbols localSymbols;
  std::vector<ReferenceHandle> references;
  std::vector<ReferenceDescription> referenceDescriptions;

  std::vector<services::Diagnostic> diagnostics;

  [[nodiscard]] bool parseSucceeded() const noexcept {
    return parseResult.fullMatch;
  }

  [[nodiscard]] bool hasAst() const noexcept {
    return parseResult.value != nullptr;
  }

  [[nodiscard]] bool parseRecovered() const noexcept {
    if (parseResult.recoveryReport.hasRecovered) {
      return true;
    }
    for (const auto &diagnostic : parseResult.parseDiagnostics) {
      if (diagnostic.isSyntax()) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] TextOffset parsedLength() const noexcept {
    return parseResult.parsedLength;
  }

  [[nodiscard]] std::shared_ptr<const TextDocument>
  textDocument() const noexcept;
  void setTextDocument(std::shared_ptr<const TextDocument> textDocument);

  [[nodiscard]] const std::string &text() const noexcept;
  [[nodiscard]] std::string_view textView() const noexcept;

  [[nodiscard]] std::uint64_t revision() const noexcept;
  [[nodiscard]] std::optional<std::int64_t> clientVersion() const noexcept;

  void setClientVersion(std::optional<std::int64_t> version) noexcept;

  void replaceText(std::string newText);
  void applyContentChanges(std::span<const DocumentContentChange> changes);
  void setText(std::string newText);

  [[nodiscard]] TextOffset positionToOffset(const text::Position &position) const;

  [[nodiscard]] TextOffset positionToOffset(std::uint32_t line,
                                            std::uint32_t character) const;

  [[nodiscard]] text::Position offsetToPosition(TextOffset offset) const;

  [[nodiscard]] SymbolId makeSymbolId(const AstNode &node) const noexcept;
  [[nodiscard]] const AstNode *findAstNode(SymbolId symbolId) const noexcept;

  Document();
  ~Document();
  Document(const Document &other)=delete;
  Document &operator=(const Document &other)=delete;
  Document(Document &&other) =delete;
  Document &operator=(Document &&other) =delete;

private:
  void resetAnalysisState() noexcept;
  void buildAstNodeIndexLocked() const;
  std::shared_ptr<const TextDocument> _textDocument;
  mutable std::mutex _astNodeIndexMutex;
  mutable std::vector<const AstNode *> _astNodesBySymbolId;
  mutable bool _astNodeIndexBuilt = false;
};

} // namespace pegium::workspace
