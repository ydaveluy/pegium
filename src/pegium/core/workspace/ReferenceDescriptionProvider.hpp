#pragma once

#include <vector>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::workspace {

/// Builds indexable reference descriptions from one document.
class ReferenceDescriptionProvider {
public:
  virtual ~ReferenceDescriptionProvider() noexcept = default;

  /// Returns every reference description extracted from `document`.
  [[nodiscard]] virtual std::vector<ReferenceDescription>
  createDescriptions(const Document &document,
                     const utils::CancellationToken &cancelToken = {}) const = 0;
};

} // namespace pegium::workspace
