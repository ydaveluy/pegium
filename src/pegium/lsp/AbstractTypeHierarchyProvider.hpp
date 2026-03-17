#pragma once

#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/services/DefaultLanguageService.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {

class AbstractTypeHierarchyProvider : protected services::DefaultLanguageService,
                                public services::TypeHierarchyProvider {
public:
  using services::DefaultLanguageService::DefaultLanguageService;

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
  [[nodiscard]] virtual std::vector<::lsp::TypeHierarchyItem>
  getSupertypes(const AstNode &node,
                const utils::CancellationToken &cancelToken) const = 0;

  [[nodiscard]] virtual std::vector<::lsp::TypeHierarchyItem>
  getSubtypes(const AstNode &node,
              const utils::CancellationToken &cancelToken) const = 0;

  virtual void customizeTypeHierarchyItem(const AstNode &,
                                          ::lsp::TypeHierarchyItem &) const {}

private:
  [[nodiscard]] std::optional<::lsp::TypeHierarchyItem>
  createTypeHierarchyItem(const AstNode &node) const;
};

} // namespace pegium::lsp
