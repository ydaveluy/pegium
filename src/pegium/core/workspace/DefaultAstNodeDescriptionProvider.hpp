#pragma once

#include <string>

#include <pegium/core/services/DefaultCoreService.hpp>
#include <pegium/core/workspace/AstNodeDescriptionProvider.hpp>

namespace pegium::services {
struct CoreServices;
}

namespace pegium::workspace {

/// Default provider that turns named AST nodes into resolvable symbol descriptions.
class DefaultAstNodeDescriptionProvider final : public AstNodeDescriptionProvider,
                                                protected services::DefaultCoreService {
public:
  using services::DefaultCoreService::DefaultCoreService;

  [[nodiscard]] std::optional<AstNodeDescription>
  createDescription(const AstNode &node, const Document &document,
                    std::string name) const override;

};

} // namespace pegium::workspace
