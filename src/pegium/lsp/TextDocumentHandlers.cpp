#include <pegium/lsp/TextDocumentHandlers.hpp>

#include <optional>
#include <utility>

#include <lsp/messages.h>

namespace pegium::lsp {

namespace {

std::vector<workspace::TextDocumentContentChange> to_text_document_changes(
    const std::vector<::lsp::TextDocumentContentChangeEvent> &changes) {
  std::vector<workspace::TextDocumentContentChange> textChanges;
  textChanges.reserve(changes.size());

  for (const auto &change : changes) {
    if (const auto *full =
            std::get_if<::lsp::TextDocumentContentChangeEvent_Text>(&change)) {
      textChanges.push_back({.range = std::nullopt, .text = full->text});
      continue;
    }

    const auto *range =
        std::get_if<::lsp::TextDocumentContentChangeEvent_Range_Text>(&change);
    if (range == nullptr) {
      continue;
    }

    textChanges.push_back(
        {.range = workspace::TextDocumentContentChangeRange(
             text::Position{range->range.start.line,
                            range->range.start.character},
             text::Position{range->range.end.line,
                            range->range.end.character}),
         .text = range->text});
  }

  return textChanges;
}

workspace::TextDocumentSaveReason
to_text_document_save_reason(const ::lsp::TextDocumentSaveReasonEnum &reason) {
  using enum ::lsp::TextDocumentSaveReason;
  switch (static_cast<::lsp::TextDocumentSaveReason>(reason)) {
  case AfterDelay:
    return workspace::TextDocumentSaveReason::AfterDelay;
  case FocusOut:
    return workspace::TextDocumentSaveReason::FocusOut;
  case Manual:
  default:
    return workspace::TextDocumentSaveReason::Manual;
  }
}

::lsp::TextEdit to_lsp_text_edit(const workspace::TextDocumentEdit &edit) {
  ::lsp::TextEdit lspEdit{};
  lspEdit.range.start = edit.range.start;
  lspEdit.range.end = edit.range.end;
  lspEdit.newText = edit.newText;
  return lspEdit;
}

} // namespace

void addTextDocumentHandlers(::lsp::MessageHandler &messageHandler,
                             workspace::TextDocuments &documents,
                             std::function<void()> ensureInitialized) {
  messageHandler.add<::lsp::notifications::TextDocument_DidOpen>(
      [&documents](::lsp::DidOpenTextDocumentParams &&params) {
        (void)documents.open(params.textDocument.uri.toString(),
                             std::move(params.textDocument.languageId),
                             std::move(params.textDocument.text),
                             params.textDocument.version);
      });

  messageHandler.add<::lsp::notifications::TextDocument_DidChange>(
      [&documents](const ::lsp::DidChangeTextDocumentParams &params) {
        (void)documents.applyContentChanges(
            params.textDocument.uri.toString(),
            to_text_document_changes(params.contentChanges),
            params.textDocument.version);
      });

  messageHandler.add<::lsp::notifications::TextDocument_DidSave>(
      [&documents](const ::lsp::DidSaveTextDocumentParams &params) {
        (void)documents.save(params.textDocument.uri.toString(),
                             params.text ? std::optional<std::string>(
                                               std::string(*params.text))
                                         : std::nullopt);
      });

  messageHandler.add<::lsp::notifications::TextDocument_DidClose>(
      [&documents](const ::lsp::DidCloseTextDocumentParams &params) {
        (void)documents.close(params.textDocument.uri.toString());
      });

  messageHandler.add<::lsp::notifications::TextDocument_WillSave>(
      [&documents, ensureInitialized = ensureInitialized](
          const ::lsp::WillSaveTextDocumentParams &params) {
        if (ensureInitialized) {
          ensureInitialized();
        }
        (void)documents.willSave(params.textDocument.uri.toString(),
                                 to_text_document_save_reason(params.reason));
      });

  messageHandler.add<::lsp::requests::TextDocument_WillSaveWaitUntil>(
      [&documents, ensureInitialized = std::move(ensureInitialized)](
          const ::lsp::WillSaveTextDocumentParams &params) {
        if (ensureInitialized) {
          ensureInitialized();
        }
        const auto edits = documents.willSaveWaitUntil(
            params.textDocument.uri.toString(),
            to_text_document_save_reason(params.reason));
        if (edits.empty()) {
          return ::lsp::TextDocument_WillSaveWaitUntilResult{};
        }

        ::lsp::Array<::lsp::TextEdit> lspEdits;
        lspEdits.reserve(edits.size());
        for (const auto &edit : edits) {
          lspEdits.push_back(to_lsp_text_edit(edit));
        }
        return ::lsp::TextDocument_WillSaveWaitUntilResult{std::move(lspEdits)};
      });
}

} // namespace pegium::lsp
