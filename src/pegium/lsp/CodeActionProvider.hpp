#pragma once

#include <optional>
#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class CodeActionProvider {
public:
  virtual ~CodeActionProvider() noexcept = default;
  virtual std::optional<
      std::vector<::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>>
  getCodeActions(const workspace::Document &document,
                 const ::lsp::CodeActionParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
