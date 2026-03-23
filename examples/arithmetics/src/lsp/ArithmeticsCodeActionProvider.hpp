#pragma once

#include <pegium/lsp/code-actions/DefaultCodeActionProvider.hpp>

namespace arithmetics::services::lsp {

class ArithmeticsCodeActionProvider final
    : public pegium::DefaultCodeActionProvider {
protected:
  void appendCodeActions(
      const pegium::workspace::Document &document,
      const ::lsp::CodeActionParams &params, CodeActionResult &actions,
      const pegium::utils::CancellationToken &cancelToken) const override;
};

} // namespace arithmetics::services::lsp
