#pragma once

#include <pegium/core/services/DefaultCoreService.hpp>
#include <pegium/core/references/References.hpp>
#include <pegium/core/workspace/IndexManager.hpp>

namespace pegium::services {
struct CoreServices;
}

namespace pegium::references {

/// Default implementation of declaration and reference queries.
class DefaultReferences : public References,
                          protected services::DefaultCoreService {
public:
  using services::DefaultCoreService::DefaultCoreService;

  /// Resolves the declarations designated by `sourceCstNode`.
  std::vector<const AstNode *>
  findDeclarations(const CstNodeView &sourceCstNode) const override;

  /// Returns the declaration CST spans designated by `sourceCstNode`.
  std::vector<CstNodeView>
  findDeclarationNodes(const CstNodeView &sourceCstNode) const override;

  /// Returns every indexed reference targeting `targetNode`.
  std::vector<workspace::ReferenceDescription>
  findReferences(const AstNode &targetNode,
                 const FindReferencesOptions &options) const override;
};

} // namespace pegium::references
