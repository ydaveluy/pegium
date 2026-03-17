#pragma once

#include <optional>
#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class DeclarationProvider {
public:
  virtual ~DeclarationProvider() noexcept = default;
  virtual std::optional<std::vector<::lsp::LocationLink>>
  getDeclaration(const workspace::Document &document,
                 const ::lsp::DeclarationParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
