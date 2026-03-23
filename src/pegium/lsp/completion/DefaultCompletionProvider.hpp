#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <lsp/types.h>

#include <pegium/core/parser/Parser.hpp>
#include <pegium/core/references/ScopeProvider.hpp>
#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>
#include <pegium/core/workspace/AstDescriptions.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Runtime context passed to each completion hook.
///
/// The parser trace computes the current completion feature, while the
/// provider adds token and AST information around the cursor.
struct CompletionContext {
  const workspace::Document &document;
  const ::lsp::CompletionParams &params;
  /// Absolute cursor offset in the document.
  TextOffset offset = 0;
  /// Offset of the current token start. Equals `offset` between tokens.
  TextOffset tokenOffset = 0;
  /// Offset of the current token end. Equals `offset` between tokens.
  TextOffset tokenEndOffset = 0;
  /// Full token text under the cursor, if any.
  std::string_view tokenText{};
  /// Slice from `tokenOffset` to `offset`.
  std::string_view prefix{};
  /// Best AST node found around the completion anchor.
  const AstNode *node = nullptr;
  /// Concrete reference under the cursor when one already exists.
  const AbstractReference *reference = nullptr;
  /// Parser expectation path currently being processed. Always valid.
  const parser::ExpectPath &feature;
};

/// Generic completion payload emitted by provider hooks.
///
/// The default provider converts it into an LSP `CompletionItem`, applies
/// fuzzy filtering on `label` or `filterText`, and fills missing metadata from
/// the active completion feature when possible.
struct CompletionValue {
  std::string label;
  std::string newText;
  std::optional<std::string> detail;
  std::optional<std::string> filterText;
  std::optional<std::string> sortText;
  std::optional<::lsp::CompletionItemKindEnum> kind;
  std::optional<::lsp::MarkupContent> documentation;
  std::optional<::lsp::TextEdit> textEdit;
  std::optional<::lsp::InsertTextFormatEnum> insertTextFormat;
  const workspace::AstNodeDescription *description = nullptr;
};

using CompletionAcceptor = std::function<void(CompletionValue value)>;

/// Generic completion provider built on top of parser completion traces.
///
/// Override the protected hooks to customize one part of the completion
/// pipeline while keeping the default generic behavior for the rest.
class DefaultCompletionProvider : protected DefaultLanguageService,
                                public ::pegium::CompletionProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

  std::optional<::lsp::CompletionList>
  getCompletion(const workspace::Document &document,
                const ::lsp::CompletionParams &params,
                const utils::CancellationToken &cancelToken) const override;

protected:
  /// Main dispatch hook called once per parser expectation alternative.
  virtual void completionFor(const CompletionContext &context,
                             const CompletionAcceptor &acceptor) const;
  /// Handles reference completion for a grammar assignment.
  virtual void completionForReference(
      const CompletionContext &context,
      const ReferenceInfo &reference,
      const CompletionAcceptor &acceptor) const;
  /// Returns the scope candidates to consider for a reference completion.
  ///
  /// This is the simplest override point when you only need to filter the
  /// default scope result.
  [[nodiscard]] virtual std::vector<const workspace::AstNodeDescription *>
  getReferenceCandidates(const CompletionContext &context,
                         const ReferenceInfo &reference) const;
  /// Builds a completion payload for one reference candidate.
  [[nodiscard]] virtual std::optional<CompletionValue>
  createReferenceCompletionItem(
      const CompletionContext &context,
      const ReferenceInfo &reference,
      const workspace::AstNodeDescription &candidate) const;
  /// Hook for parser rule completion. The default implementation emits no item.
  virtual void completionForRule(const CompletionContext &context,
                                 const grammar::AbstractRule &rule,
                                 const CompletionAcceptor &acceptor) const;
  /// Hook for keyword completion.
  virtual void completionForKeyword(const CompletionContext &context,
                                    const grammar::Literal &keyword,
                                    const CompletionAcceptor &acceptor) const;
  /// Cheap keyword-level filter executed before item creation.
  [[nodiscard]] virtual bool
  filterKeyword(const CompletionContext &context,
                const grammar::Literal &keyword) const;
  /// Final transformation step from `CompletionValue` to LSP item.
  [[nodiscard]] virtual bool
  fillCompletionItem(const CompletionContext &context,
                     const CompletionValue &value,
                     ::lsp::CompletionItem &item) const;
  /// Returns whether the provider should continue with later parser features.
  [[nodiscard]] virtual bool
  continueCompletion(const CompletionContext &context) const;

  [[nodiscard]] ReferenceInfo
  makeReferenceInfo(const CompletionContext &context,
                    const grammar::Assignment &assignment) const;
};

} // namespace pegium
