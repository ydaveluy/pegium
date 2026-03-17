#pragma once

#include <pegium/services/DefaultCoreService.hpp>
#include <pegium/workspace/ReferenceDescriptionProvider.hpp>

namespace pegium::services {
struct CoreServices;
}

namespace pegium::workspace {

class DefaultReferenceDescriptionProvider final
    : public ReferenceDescriptionProvider,
      protected services::DefaultCoreService {
public:
  using services::DefaultCoreService::DefaultCoreService;

  [[nodiscard]] std::vector<ReferenceDescription>
  createDescriptions(const Document &document,
                     const utils::CancellationToken &cancelToken = {}) const override;
};

} // namespace pegium::workspace
