#pragma once

#include <pegium/core/documentation/DocumentationProvider.hpp>
#include <pegium/core/services/DefaultCoreService.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <optional>
#include <string>

#include <pegium/core/workspace/Symbol.hpp>

namespace pegium::documentation {

/// Default documentation renderer for JSDoc-style comments.
class DefaultDocumentationProvider : public DocumentationProvider,
                                     protected services::DefaultCoreService {
public:
  using services::DefaultCoreService::DefaultCoreService;

  ~DefaultDocumentationProvider() override = default;
  /// Returns the Markdown documentation derived from the comment attached to `node`.
  [[nodiscard]] std::optional<std::string>
  getDocumentation(const AstNode &node) const override;

protected:
  /// Renders one `{@link ...}` occurrence found in the documentation comment.
  [[nodiscard]] virtual std::optional<std::string>
  documentationLinkRenderer(const AstNode &node, std::string_view name,
                            std::string_view display) const;
  /// Renders one JSDoc tag line such as `@deprecated`.
  [[nodiscard]] virtual std::optional<std::string>
  documentationTagRenderer(const AstNode &node, std::string_view tag) const;
  /// Rewrites every `{@link ...}` occurrence in `line`.
  [[nodiscard]] std::string normalizeJsdocLinks(const AstNode &node,
                                                std::string line) const;
  /// Looks up `name` in lexical local symbols visible from `node`.
  [[nodiscard]] virtual std::optional<workspace::AstNodeDescription>
  findNameInLocalSymbols(const AstNode &node, std::string_view name) const;
  /// Looks up `name` in globally indexed exported symbols.
  [[nodiscard]] virtual std::optional<workspace::AstNodeDescription>
  findNameInGlobalScope(const AstNode &node, std::string_view name) const;
};

} // namespace pegium::documentation
