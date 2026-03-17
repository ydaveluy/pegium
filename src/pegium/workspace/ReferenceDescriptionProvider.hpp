#pragma once

#include <vector>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::workspace {

class ReferenceDescriptionProvider {
public:
  virtual ~ReferenceDescriptionProvider() noexcept = default;

  [[nodiscard]] virtual std::vector<ReferenceDescription>
  createDescriptions(const Document &document,
                     const utils::CancellationToken &cancelToken = {}) const = 0;
};

} // namespace pegium::workspace
