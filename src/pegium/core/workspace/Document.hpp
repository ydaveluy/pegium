#pragma once

#include <algorithm>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include <pegium/core/parser/Parser.hpp>
#include <pegium/core/services/Diagnostic.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>
#include <pegium/core/syntax-tree/Reference.hpp>
#include <pegium/core/text/Position.hpp>
#include <pegium/core/workspace/LocalSymbols.hpp>
#include <pegium/core/workspace/Symbol.hpp>
#include <pegium/core/workspace/TextDocument.hpp>

namespace pegium::workspace {

class DocumentFactory;

/// Progressive analysis phase reached by a managed document.
enum class DocumentState : std::uint8_t {
  Changed,
  Parsed,
  IndexedContent,
  ComputedScopes,
  Linked,
  IndexedReferences,
  Validated,
};

/// In-memory document state shared by parsing, indexing, linking, and validation.
struct Document {
  friend class DocumentFactory;

  DocumentId id = InvalidDocumentId;
  /// Canonical document URI. Managed workspace documents keep it stable for
  /// their whole lifetime.
  const std::string uri;

  // Atomic so the build can advance it while readers (e.g. waitUntil, LSP
  // request gating) observe it concurrently without a data race. Transitions are
  // ordered by the build's phase barriers and the phase-listener handshake; the
  // atomic only removes the formal race on the byte itself.
  std::atomic<DocumentState> state = DocumentState::Changed;

  parser::ParseResult parseResult;
  LocalSymbols localSymbols;

  std::vector<pegium::Diagnostic> diagnostics;

  /// Returns whether parsing reached a full grammar match.
  [[nodiscard]] bool parseSucceeded() const noexcept {
    return parseResult.fullMatch;
  }

  /// Returns whether parsing produced an AST root.
  [[nodiscard]] bool hasAst() const noexcept {
    return parseResult.value != nullptr;
  }

  /// Returns whether parsing used recovery or reported syntax diagnostics.
  [[nodiscard]] bool parseRecovered() const noexcept {
    if (parseResult.recoveryReport.hasRecovered) {
      return true;
    }
    return std::ranges::any_of(parseResult.parseDiagnostics,
                               [](const auto &diagnostic) {
                                 return diagnostic.isSyntax();
                               });
  }

  /// Returns the number of source bytes consumed by the parse result.
  [[nodiscard]] TextOffset parsedLength() const noexcept {
    return parseResult.parsedLength;
  }

  /// Returns the backing text document. Never null by design.
  [[nodiscard]] const TextDocument &textDocument() const noexcept;

  /// Returns the symbol identifier of `node` inside this document.
  [[nodiscard]] SymbolId makeSymbolId(const AstNode &node) const noexcept;
  /// Resolves a symbol identifier previously created by `makeSymbolId(...)` to
  /// its AST node.
  ///
  /// Throws `utils::MissingAstDocumentError` when the document currently owns no
  /// AST or `symbolId` does not resolve to a live node. Use `findAstNode(...)`
  /// when an absent node is an expected outcome (it returns `nullptr`).
  [[nodiscard]] const AstNode &getAstNode(SymbolId symbolId) const;
  /// Resolves a symbol identifier previously created by `makeSymbolId(...)`,
  /// returning `nullptr` when `symbolId` is invalid, the document owns no AST,
  /// or the id no longer resolves to a live node. Use `getAstNode(...)` to throw
  /// instead of returning null.
  [[nodiscard]] const AstNode *findAstNode(SymbolId symbolId) const noexcept;

  /// Creates a document backed by `textDocument`.
  ///
  /// When `uri` is empty, the attached text-document URI becomes the document
  /// URI. The resolved URI then stays stable for the lifetime of the document.
  explicit Document(std::shared_ptr<TextDocument> textDocument,
                    const std::string &uri = {});
  ~Document();
  Document(const Document &other)=delete;
  Document &operator=(const Document &other)=delete;
  Document(Document &&other) =delete;
  Document &operator=(Document &&other) =delete;

private:
  void attachTextDocument(std::shared_ptr<TextDocument> textDocument);
  void resetAnalysisState() noexcept;
  std::shared_ptr<TextDocument> _textDocument;
};

} // namespace pegium::workspace
