#include "lsp/ArithmeticsCodeActionProvider.hpp"

#include <format>
#include <string>
#include <string_view>

namespace arithmetics::services::lsp {

void ArithmeticsCodeActionProvider::appendCodeActions(
    const pegium::workspace::Document &document,
    const ::lsp::CodeActionParams &params,
    CodeActionResult &actions,
    const pegium::utils::CancellationToken &cancelToken) const {
  pegium::utils::throw_if_cancelled(cancelToken);

  const auto begin = document.positionToOffset(params.range.start);
  const auto end = document.positionToOffset(params.range.end);

  for (const auto &diagnostic : params.context.diagnostics) {
    if (!diagnostic.code.has_value() ||
        !std::holds_alternative<::lsp::String>(*diagnostic.code) ||
        std::get<::lsp::String>(*diagnostic.code) !=
            "arithmetics.expression-normalizable" ||
        !diagnostic.data.has_value() || !diagnostic.data->isObject()) {
      continue;
    }

    const auto *constant = diagnostic.data->object().find("constant");
    if (constant == nullptr || !constant->isNumber()) {
      continue;
    }

    const auto replaceBegin = begin;
    const auto replaceEnd = end > begin ? end : begin;
    if (replaceEnd <= replaceBegin ||
        replaceEnd >
            static_cast<pegium::TextOffset>(document.text().size())) {
      continue;
    }

    ::lsp::CodeAction action{};
    action.title = std::format("Replace with constant {}", constant->number());
    action.kind = ::lsp::CodeActionKind::QuickFix;
    action.isPreferred = true;

    ::lsp::WorkspaceEdit edit{};
    ::lsp::Map<::lsp::DocumentUri, ::lsp::Array<::lsp::TextEdit>> changes;
    ::lsp::TextEdit textEdit{};
    textEdit.range.start = document.offsetToPosition(replaceBegin);
    textEdit.range.end = document.offsetToPosition(replaceEnd);
    textEdit.newText = std::format("{}", constant->number());
    auto &documentChanges = changes[::lsp::Uri::parse(document.uri)];
    documentChanges.push_back(std::move(textEdit));
    edit.changes = std::move(changes);
    action.edit = std::move(edit);
    actions.push_back(std::move(action));
  }
}

} // namespace arithmetics::services::lsp
