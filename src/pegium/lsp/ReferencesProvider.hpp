#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class ReferencesProvider {
public:
  virtual ~ReferencesProvider() noexcept = default;
  virtual std::vector<::lsp::Location>
  findReferences(const workspace::Document &document,
                 const ::lsp::ReferenceParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
