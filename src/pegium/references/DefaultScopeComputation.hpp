#pragma once

#include <pegium/services/DefaultCoreService.hpp>
#include <pegium/references/ScopeComputation.hpp>
#include <pegium/workspace/AstNodeDescriptionProvider.hpp>

namespace pegium::services {
struct CoreServices;
}

namespace pegium::references {

class DefaultScopeComputation : public ScopeComputation,
                                protected services::DefaultCoreService {
public:
  using services::DefaultCoreService::DefaultCoreService;

  std::vector<workspace::AstNodeDescription> collectExportedSymbols(
      const workspace::Document &document,
      const utils::CancellationToken &cancelToken) const override;
  workspace::LocalSymbols collectLocalSymbols(
      const workspace::Document &document,
      const utils::CancellationToken &cancelToken) const override;

};

} // namespace pegium::references
