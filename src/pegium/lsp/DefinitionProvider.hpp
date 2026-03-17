#pragma once

#include <optional>
#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class DefinitionProvider {
public:
  virtual ~DefinitionProvider() noexcept = default;
  virtual std::optional<std::vector<::lsp::LocationLink>>
  getDefinition(const workspace::Document &document,
                const ::lsp::DefinitionParams &params,
                const utils::CancellationToken &cancelToken =
                    utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
