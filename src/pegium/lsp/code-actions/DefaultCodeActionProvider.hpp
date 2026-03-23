#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <lsp/types.h>

#include <pegium/lsp/code-actions/CodeActionProvider.hpp>

namespace pegium {

/// Default code action provider that turns diagnostic metadata into quick fixes.
class DefaultCodeActionProvider : public CodeActionProvider {
public:
  /// Returns the default quick fixes plus any actions appended by overrides.
  std::optional<std::vector<::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>>
  getCodeActions(const workspace::Document &document,
                 const ::lsp::CodeActionParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token) const override;

protected:
  using CodeActionResult =
      std::vector<::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>;

  /// Appends code actions for `params` into `actions`.
  virtual void appendCodeActions(
      const workspace::Document &document, const ::lsp::CodeActionParams &params,
      CodeActionResult &actions,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const;

  /// Internal normalized edit description extracted from one diagnostic.
  struct DefaultCodeActionEdit {
    std::string kind;
    std::string editKind;
    std::string title;
    std::string newText;
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
  };

  /// Extracts default quick-fix edits encoded on a diagnostic.
  [[nodiscard]] static std::optional<std::vector<DefaultCodeActionEdit>>
  extractDefaultCodeActions(const ::lsp::Diagnostic &diagnostic);

  /// Builds one LSP code action from a normalized default edit.
  [[nodiscard]] static std::optional<::lsp::CodeAction>
  makeDefaultCodeAction(const workspace::Document &document,
                        const ::lsp::Diagnostic &diagnostic,
                        const DefaultCodeActionEdit &edit);

};

} // namespace pegium
