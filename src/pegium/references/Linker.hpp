#pragma once

#include <pegium/syntax-tree/Reference.hpp>
#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/AstDescriptions.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::references {

class Linker {
public:
  virtual ~Linker() noexcept = default;
  virtual void
  link(workspace::Document &document,
       const utils::CancellationToken &cancelToken = {}) const = 0;

  virtual void
  unlink(workspace::Document &document,
         const utils::CancellationToken &cancelToken = {}) const;

  virtual workspace::AstNodeDescriptionOrError
  getCandidate(const ReferenceInfo &reference) const;

  virtual workspace::AstNodeDescriptionsOrError
  getCandidates(const ReferenceInfo &reference) const;

  virtual ReferenceResolution resolve(const AbstractReference &reference) const;

  virtual MultiReferenceResolution
  resolveAll(const AbstractReference &reference) const;
};

} // namespace pegium::references
