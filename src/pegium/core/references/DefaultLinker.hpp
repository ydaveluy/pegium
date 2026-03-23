#pragma once

#include <optional>

#include <pegium/core/services/DefaultCoreService.hpp>
#include <pegium/core/references/Linker.hpp>
#include <pegium/core/references/NameProvider.hpp>
#include <pegium/core/references/ScopeProvider.hpp>
#include <pegium/core/workspace/Documents.hpp>
#include <pegium/core/workspace/IndexManager.hpp>

namespace pegium::services {
struct CoreServices;
}

namespace pegium::references {

/// Default linker backed by the registered scope provider and document index.
class DefaultLinker : public Linker,
                      protected services::DefaultCoreService {
public:
  /// Creates a linker using the reference and workspace services from `services`.
  explicit DefaultLinker(const services::CoreServices &services);

  /// Resolves and caches every reference owned by `document`.
  void link(workspace::Document &document,
            const utils::CancellationToken &cancelToken) const override;

  /// Clears every cached resolution for `document`.
  void unlink(workspace::Document &document) const override;

  /// Returns the first visible candidate description for `reference`.
  workspace::AstNodeDescriptionOrError
  getCandidate(const ReferenceInfo &reference) const override;

  /// Returns every visible candidate description for `reference`.
  workspace::AstNodeDescriptionsOrError
  getCandidates(const ReferenceInfo &reference) const override;

  /// Resolves `reference` to a live AST node when possible.
  workspace::ResolvedAstNodeDescriptionOrError
  resolve(const AbstractSingleReference &reference) const override;

  /// Resolves every candidate of `reference` to live AST nodes when possible.
  workspace::ResolvedAstNodeDescriptionsOrError
  resolveAll(const AbstractMultiReference &reference) const override;

private:
  [[nodiscard]] workspace::LinkingError
  createLinkingError(const ReferenceInfo &reference,
                     std::optional<workspace::AstNodeDescription>
                         targetDescription = std::nullopt) const;
  [[nodiscard]] workspace::LinkingError
  createCycleLinkingError(const ReferenceInfo &reference) const;
  [[nodiscard]] workspace::LinkingError
  createExceptionLinkingError(const ReferenceInfo &reference,
                              std::string message) const;
  [[nodiscard]] workspace::ResolvedAstNodeDescriptionOrError
  getLinkedNode(const ReferenceInfo &reference,
                const workspace::Document &currentDocument) const;
  [[nodiscard]] workspace::ResolvedAstNodeDescriptionsOrError
  getLinkedNodes(const ReferenceInfo &reference,
                 const workspace::Document &currentDocument) const;
};

} // namespace pegium::references
