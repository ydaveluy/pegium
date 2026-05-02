#pragma once

#include <pegium/core/services/DefaultCoreService.hpp>
#include <pegium/core/workspace/AstNodeDescriptionProvider.hpp>

namespace pegium {
struct CoreServices;
}

namespace pegium::workspace {

/// Default provider that turns named AST nodes into resolvable symbol descriptions.
class DefaultAstNodeDescriptionProvider final : public AstNodeDescriptionProvider,
                                                protected pegium::DefaultCoreService {
public:
  using pegium::DefaultCoreService::DefaultCoreService;

  [[nodiscard]] std::optional<AstNodeDescription>
  createDescription(const AstNode &node, const Document &document,
                    references::AstNodeName nameInfo) const override;
};

} // namespace pegium::workspace
