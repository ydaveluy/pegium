#pragma once

#include <memory>
#include <mutex>
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

  DocumentState state = DocumentState::Changed;

  parser::ParseResult parseResult;
  LocalSymbols localSymbols;
  /// Concrete reference handles collected during parsing. Entries are never
  /// null and always point to reference objects owned by this document AST.
  std::vector<ReferenceHandle> references;

  std::vector<services::Diagnostic> diagnostics;

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
    for (const auto &diagnostic : parseResult.parseDiagnostics) {
      if (diagnostic.isSyntax()) {
        return true;
      }
    }
    return false;
  }

  /// Returns the number of source bytes consumed by the parse result.
  [[nodiscard]] TextOffset parsedLength() const noexcept {
    return parseResult.parsedLength;
  }

  /// Returns the backing text document. Never null by design.
  [[nodiscard]] const TextDocument &textDocument() const noexcept;

  /// Returns the symbol identifier of `node` inside this document.
  [[nodiscard]] SymbolId makeSymbolId(const AstNode &node) const noexcept;
  /// Resolves a symbol identifier previously created by `makeSymbolId(...)`.
  ///
  /// `symbolId` must be valid, the document must currently own an AST, and
  /// the symbol must still resolve to a live node.
  [[nodiscard]] const AstNode &getAstNode(SymbolId symbolId) const noexcept;
  /// Resolves a symbol identifier previously created by `makeSymbolId(...)`.
  /// `symbolId` must be valid and the document must currently own an AST.
  [[nodiscard]] const AstNode *findAstNode(SymbolId symbolId) const noexcept;

  /// Creates a document backed by `textDocument`.
  ///
  /// When `uri` is empty, the attached text-document URI becomes the document
  /// URI. The resolved URI then stays stable for the lifetime of the document.
  explicit Document(std::shared_ptr<TextDocument> textDocument,
                    std::string uri = {});
  ~Document();
  Document(const Document &other)=delete;
  Document &operator=(const Document &other)=delete;
  Document(Document &&other) =delete;
  Document &operator=(Document &&other) =delete;

private:
  void attachTextDocument(std::shared_ptr<TextDocument> textDocument);
  void resetAnalysisState() noexcept;
  void buildAstNodeIndexLocked() const;
  std::shared_ptr<TextDocument> _textDocument;
  mutable std::mutex _astNodeIndexMutex;
  mutable std::vector<const AstNode *> _astNodesBySymbolId;
  mutable bool _astNodeIndexBuilt = false;
};

} // namespace pegium::workspace
