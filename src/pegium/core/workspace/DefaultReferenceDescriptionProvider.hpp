#pragma once

#include <pegium/core/services/DefaultCoreService.hpp>
#include <pegium/core/workspace/ReferenceDescriptionProvider.hpp>

namespace pegium {
struct CoreServices;
}

namespace pegium::workspace {

/// Default provider extracting reference descriptions from one document AST.
class DefaultReferenceDescriptionProvider final
    : public ReferenceDescriptionProvider,
      protected pegium::DefaultCoreService {
public:
  using pegium::DefaultCoreService::DefaultCoreService;

  [[nodiscard]] std::vector<ReferenceDescription>
  createDescriptions(const Document &document,
                     const utils::CancellationToken &cancelToken = {}) const override;
};

} // namespace pegium::workspace
