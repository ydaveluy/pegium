#pragma once

#include <pegium/documentation/DocumentationProvider.hpp>
#include <pegium/services/DefaultCoreService.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <optional>
#include <string>

#include <pegium/workspace/Symbol.hpp>

namespace pegium::documentation {


class DefaultDocumentationProvider : public DocumentationProvider,
                                     protected services::DefaultCoreService {
public:
  using services::DefaultCoreService::DefaultCoreService;

  ~DefaultDocumentationProvider() override = default;
  [[nodiscard]] std::optional<std::string>
  getDocumentation(const AstNode &node) const override;

protected:
  [[nodiscard]] virtual std::optional<std::string>
  documentationLinkRenderer(const AstNode &node, std::string_view name,
                            std::string_view display) const;
  [[nodiscard]] virtual std::optional<std::string>
  documentationTagRenderer(const AstNode &node, std::string_view tag) const;
  [[nodiscard]] std::string normalizeJsdocLinks(const AstNode &node,
                                                std::string line) const;
  [[nodiscard]] virtual std::optional<workspace::AstNodeDescription>
  findNameInLocalSymbols(const AstNode &node, std::string_view name) const;
  [[nodiscard]] virtual std::optional<workspace::AstNodeDescription>
  findNameInGlobalScope(const AstNode &node, std::string_view name) const;
};

} // namespace pegium::documentation
