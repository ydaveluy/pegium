#pragma once

#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {

/// Shared type-hierarchy provider that resolves one node then delegates edge collection.
class AbstractTypeHierarchyProvider : protected DefaultLanguageService,
                                public ::pegium::TypeHierarchyProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

  std::vector<::lsp::TypeHierarchyItem>
  prepareTypeHierarchy(const workspace::Document &document,
                       const ::lsp::TypeHierarchyPrepareParams &params,
                       const utils::CancellationToken &cancelToken) const override;

  std::vector<::lsp::TypeHierarchyItem>
  supertypes(const ::lsp::TypeHierarchySupertypesParams &params,
             const utils::CancellationToken &cancelToken) const override;

  std::vector<::lsp::TypeHierarchyItem>
  subtypes(const ::lsp::TypeHierarchySubtypesParams &params,
           const utils::CancellationToken &cancelToken) const override;

protected:
  /// Returns direct or effective supertypes of `node`.
  [[nodiscard]] virtual std::vector<::lsp::TypeHierarchyItem>
  getSupertypes(const AstNode &node,
                const utils::CancellationToken &cancelToken) const = 0;

  /// Returns direct or effective subtypes of `node`.
  [[nodiscard]] virtual std::vector<::lsp::TypeHierarchyItem>
  getSubtypes(const AstNode &node,
              const utils::CancellationToken &cancelToken) const = 0;

  /// Customizes the item built for `node` before it is returned.
  virtual void customizeTypeHierarchyItem(const AstNode &,
                                          ::lsp::TypeHierarchyItem &) const {}

private:
  [[nodiscard]] std::optional<::lsp::TypeHierarchyItem>
  createTypeHierarchyItem(const AstNode &node) const;
};

} // namespace pegium
