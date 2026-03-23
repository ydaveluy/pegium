#pragma once

#include <vector>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/AstDescriptions.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::references {

/// Builds the exported and local symbol tables derived from one document AST.
class ScopeComputation {
public:
  virtual ~ScopeComputation() noexcept = default;

  /// Returns the symbols of `document` that should be visible from other documents.
  virtual std::vector<workspace::AstNodeDescription>
  collectExportedSymbols(const workspace::Document &document,
                         const utils::CancellationToken &cancelToken) const = 0;

  /// Returns the symbols of `document` that should be visible from nested AST scopes.
  virtual workspace::LocalSymbols
  collectLocalSymbols(const workspace::Document &document,
                      const utils::CancellationToken &cancelToken) const = 0;
};

} // namespace pegium::references
