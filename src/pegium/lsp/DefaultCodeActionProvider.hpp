#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <lsp/types.h>

#include <pegium/lsp/CodeActionProvider.hpp>

namespace pegium::services {

class DefaultCodeActionProvider : public CodeActionProvider {
public:
  std::optional<std::vector<::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>>
  getCodeActions(const workspace::Document &document,
                 const ::lsp::CodeActionParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token) const override;

protected:
  using CodeActionResult =
      std::vector<::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>;

  virtual void appendCodeActions(
      const workspace::Document &document, const ::lsp::CodeActionParams &params,
      CodeActionResult &actions,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const;

  struct DefaultCodeActionEdit {
    std::string kind;
    std::string editKind;
    std::string title;
    std::string newText;
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
  };

  [[nodiscard]] static std::optional<std::vector<DefaultCodeActionEdit>>
  extractDefaultCodeActions(const ::lsp::Diagnostic &diagnostic);

  [[nodiscard]] static std::optional<::lsp::CodeAction>
  makeDefaultCodeAction(const workspace::Document &document,
                        const ::lsp::Diagnostic &diagnostic,
                        const DefaultCodeActionEdit &edit);

  [[nodiscard]] std::optional<std::vector<
      ::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>>
  makeCodeActionsResult(CodeActionResult actions) const;
};

} // namespace pegium::services
