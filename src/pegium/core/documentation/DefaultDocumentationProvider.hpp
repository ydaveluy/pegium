#pragma once

#include <pegium/core/documentation/DocumentationProvider.hpp>
#include <pegium/core/services/DefaultCoreService.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <optional>
#include <string>

#include <pegium/core/workspace/Symbol.hpp>

namespace pegium::documentation {

/// Default renderer for documentation comments.
class DefaultDocumentationProvider : public DocumentationProvider,
                                     protected pegium::DefaultCoreService {
public:
  using pegium::DefaultCoreService::DefaultCoreService;

  ~DefaultDocumentationProvider() override = default;
  // Keep the grammar-element overload (keyword documentation) visible alongside
  // the AstNode override below.
  using DocumentationProvider::getDocumentation;
  /// Returns the Markdown documentation derived from the comment attached to `node`.
  [[nodiscard]] std::optional<std::string>
  getDocumentation(const AstNode &node) const override;

protected:
  /// Renders one `{@link ...}` occurrence, given its split target and display.
  ///
  /// The default implementation resolves `name` to a declaration visible from
  /// `node` and links to its source location, or returns `std::nullopt` when it
  /// cannot be resolved.
  [[nodiscard]] virtual std::optional<std::string>
  documentationLinkRenderer(const AstNode &node, std::string_view name,
                            std::string_view display) const;
  /// Renders one documentation tag (a block tag or a non-link inline tag).
  ///
  /// `content` is the already-rendered Markdown content of the tag. The default
  /// implementation returns `std::nullopt`, which falls back to the standard
  /// `*@tag* — content` rendering.
  [[nodiscard]] virtual std::optional<std::string>
  documentationTagRenderer(const AstNode &node, std::string_view name,
                           std::string_view content, bool inlineTag) const;
  /// Looks up `name` in lexical local symbols visible from `node`.
  [[nodiscard]] virtual std::optional<workspace::AstNodeDescription>
  findNameInLocalSymbols(const AstNode &node, std::string_view name) const;
  /// Looks up `name` in globally indexed exported symbols.
  [[nodiscard]] virtual std::optional<workspace::AstNodeDescription>
  findNameInGlobalScope(const AstNode &node, std::string_view name) const;
};

} // namespace pegium::documentation
