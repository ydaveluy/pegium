#pragma once

#include <pegium/services/DefaultCoreService.hpp>
#include <pegium/references/Linker.hpp>
#include <pegium/references/NameProvider.hpp>
#include <pegium/references/ScopeProvider.hpp>
#include <pegium/workspace/Documents.hpp>
#include <pegium/workspace/IndexManager.hpp>

namespace pegium::services {
struct CoreServices;
}

namespace pegium::references {

class DefaultLinker : public Linker,
                      protected services::DefaultCoreService {
public:
  using services::DefaultCoreService::DefaultCoreService;

  void link(workspace::Document &document,
            const utils::CancellationToken &cancelToken) const override;

  void unlink(workspace::Document &document,
              const utils::CancellationToken &cancelToken) const override;

  workspace::AstNodeDescriptionOrError
  getCandidate(const ReferenceInfo &reference) const override;

  workspace::AstNodeDescriptionsOrError
  getCandidates(const ReferenceInfo &reference) const override;

  ReferenceResolution resolve(const AbstractReference &reference) const override;

  MultiReferenceResolution
  resolveAll(const AbstractReference &reference) const override;

private:
  [[nodiscard]] AstNode *loadAstNode(
      const workspace::AstNodeDescription &description) const;
  [[nodiscard]] workspace::LinkingError
  createLinkingError(const ReferenceInfo &reference, std::string message,
                     std::optional<workspace::AstNodeDescription>
                         targetDescription = std::nullopt) const;
};

} // namespace pegium::references
