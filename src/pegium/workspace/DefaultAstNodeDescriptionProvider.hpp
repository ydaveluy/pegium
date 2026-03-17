#pragma once

#include <optional>
#include <string>

#include <pegium/services/DefaultCoreService.hpp>
#include <pegium/workspace/AstNodeDescriptionProvider.hpp>

namespace pegium::services {
struct CoreServices;
}

namespace pegium::workspace {

class DefaultAstNodeDescriptionProvider final : public AstNodeDescriptionProvider,
                                                protected services::DefaultCoreService {
public:
  using services::DefaultCoreService::DefaultCoreService;

  [[nodiscard]] std::optional<AstNodeDescription>
  createDescription(const AstNode &node, const Document &document,
                    std::string name = {}) const override;

};

} // namespace pegium::workspace
