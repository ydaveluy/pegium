#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class CodeLensProvider {
public:
  virtual ~CodeLensProvider() noexcept = default;
  virtual std::vector<::lsp::CodeLens>
  provideCodeLens(const workspace::Document &document,
                  const ::lsp::CodeLensParams &params,
                  const utils::CancellationToken &cancelToken =
                      utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
