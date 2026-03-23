#pragma once

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/AstDescriptions.hpp>

namespace pegium {

class AbstractSingleReference;
class AbstractMultiReference;
struct ReferenceInfo;

namespace workspace {
class Document;
}

namespace references {

/// Resolves AST references for one document and exposes low-level lookup helpers.
class Linker {
public:
  virtual ~Linker() noexcept = default;

  /// Resolves every reference owned by `document`.
  ///
  /// Implementations may reuse cached results already stored on reference
  /// objects. `cancelToken` should be polled during long-running work.
  virtual void
  link(workspace::Document &document,
       const utils::CancellationToken &cancelToken = {}) const = 0;

  /// Clears every cached resolution previously produced for `document`.
  virtual void unlink(workspace::Document &document) const = 0;

  /// Returns the best matching description for one reference lookup.
  virtual workspace::AstNodeDescriptionOrError
  getCandidate(const ReferenceInfo &reference) const = 0;

  /// Returns every visible candidate description for one reference lookup.
  virtual workspace::AstNodeDescriptionsOrError
  getCandidates(const ReferenceInfo &reference) const = 0;

  /// Resolves a single-valued reference to a live AST node and its description.
  virtual workspace::ResolvedAstNodeDescriptionOrError
  resolve(const AbstractSingleReference &reference) const = 0;

  /// Resolves a multi-valued reference to every live AST node that is currently visible.
  virtual workspace::ResolvedAstNodeDescriptionsOrError
  resolveAll(const AbstractMultiReference &reference) const = 0;
};

} // namespace references

} // namespace pegium
