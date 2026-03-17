#pragma once

#include <vector>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/AstDescriptions.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::references {

class ScopeComputation {
public:
  virtual ~ScopeComputation() noexcept = default;
  virtual std::vector<workspace::AstNodeDescription>
  collectExportedSymbols(const workspace::Document &document,
                         const utils::CancellationToken &cancelToken) const = 0;

  virtual workspace::LocalSymbols
  collectLocalSymbols(const workspace::Document &document,
                      const utils::CancellationToken &cancelToken) const = 0;
};

} // namespace pegium::references
