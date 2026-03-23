#pragma once

#include <pegium/core/services/DefaultCoreService.hpp>
#include <pegium/core/references/NameProvider.hpp>
#include <pegium/core/references/ScopeComputation.hpp>
#include <pegium/core/workspace/AstNodeDescriptionProvider.hpp>

namespace pegium::services {
struct CoreServices;
}

namespace pegium::references {

/// Default symbol collection strategy used during document indexing.
class DefaultScopeComputation : public ScopeComputation,
                                protected services::DefaultCoreService {
public:
  using services::DefaultCoreService::DefaultCoreService;

  /// Collects exported symbols from the document root and its direct AST content.
  std::vector<workspace::AstNodeDescription> collectExportedSymbols(
      const workspace::Document &document,
      const utils::CancellationToken &cancelToken) const override;

  /// Collects local symbols from every non-root AST node in the document.
  workspace::LocalSymbols collectLocalSymbols(
      const workspace::Document &document,
      const utils::CancellationToken &cancelToken) const override;

protected:
  /// Customizes how exported symbols are gathered from `parentNode`.
  [[nodiscard]] virtual std::vector<workspace::AstNodeDescription>
  collectExportedSymbolsForNode(
      const AstNode &parentNode, const workspace::Document &document,
      const utils::CancellationToken &cancelToken) const;

  /// Customizes how local symbols are gathered under `rootNode`.
  virtual void collectLocalSymbolsForNode(
      const AstNode &rootNode, const workspace::Document &document,
      workspace::LocalSymbols &symbols,
      const utils::CancellationToken &cancelToken) const;

  /// Adds one exported symbol for `node` when it is named and describable.
  virtual void addExportedSymbol(
      const AstNode &node, std::vector<workspace::AstNodeDescription> &exports,
      const workspace::Document &document) const;

  /// Adds one local symbol for `node` under its direct container when possible.
  virtual void addLocalSymbol(const AstNode &node,
                              const workspace::Document &document,
                              workspace::LocalSymbols &symbols) const;

};

} // namespace pegium::references
