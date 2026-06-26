#include <arithmetics/lsp/ArithmeticsCodeActionProvider.hpp>

#include <arithmetics/core/validation/ArithmeticsValidator.hpp>

#include <format>
#include <string>
#include <string_view>

namespace arithmetics {

void ArithmeticsCodeActionProvider::appendCodeActions(
    const pegium::workspace::Document &document,
    const ::lsp::CodeActionParams &params,
    CodeActionResult &actions,
    const pegium::utils::CancellationToken &cancelToken) const {
  pegium::utils::throw_if_cancelled(cancelToken);

  const auto &textDocument = document.textDocument();

  for (const auto &diagnostic : params.context.diagnostics) {
    if (!diagnostic.code.has_value() ||
        !std::holds_alternative<::lsp::String>(*diagnostic.code) ||
        std::get<::lsp::String>(*diagnostic.code) !=
            std::string(
                arithmetics::validation::IssueCodes::ExpressionNormalizable) ||
        !diagnostic.data.has_value() || !diagnostic.data->isObject()) {
      continue;
    }

    const auto *constant = diagnostic.data->object().find("constant");
    if (constant == nullptr || !constant->isNumber()) {
      continue;
    }

    // Size the edit from the diagnostic's own range, not the request's
    // cursor/selection range (which is empty on a cursor-only invocation).
    const auto replaceBegin = textDocument.offsetAt(diagnostic.range.start);
    const auto replaceEnd = textDocument.offsetAt(diagnostic.range.end);
    if (replaceEnd <= replaceBegin ||
        replaceEnd > static_cast<pegium::TextOffset>(textDocument.getText().size())) {
      continue;
    }

    ::lsp::CodeAction action{};
    action.title = std::format("Replace with constant {}", constant->number());
    action.kind = ::lsp::CodeActionKind::QuickFix;
    action.isPreferred = true;

    ::lsp::WorkspaceEdit edit{};
    ::lsp::Map<::lsp::DocumentUri, ::lsp::Array<::lsp::TextEdit>> changes;
    ::lsp::TextEdit textEdit{};
    textEdit.range = diagnostic.range;
    textEdit.newText = std::format("{}", constant->number());
    auto &documentChanges = changes[::lsp::Uri::parse(document.uri)];
    documentChanges.push_back(std::move(textEdit));
    edit.changes = std::move(changes);
    action.edit = std::move(edit);
    actions.push_back(std::move(action));
  }
}

} // namespace arithmetics
