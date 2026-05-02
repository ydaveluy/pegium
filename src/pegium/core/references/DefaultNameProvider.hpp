#pragma once

#include <pegium/core/references/NameProvider.hpp>

namespace pegium::references {

/// Default name provider that prefers `pegium::NamedAstNode::name` and falls
/// back to the grammar assignment named `name`.
class DefaultNameProvider : public NameProvider {
public:
  /// Returns the textual name and its CST source for `node`. Uses
  /// `NamedAstNode::name` when available, otherwise extracts the value of
  /// the `name` assignment via the grammar's `FeatureValue`.
  [[nodiscard]] AstNodeName nameOf(const AstNode &node) const override;

  /// Override of `NameProvider::getName` that skips the CST walk for
  /// `NamedAstNode` instances. Indexing/scope-computation that need both
  /// the name AND its CST source should call `nameOf` instead; this fast
  /// path matters for the (hot) qualified-name resolution loop in scope
  /// providers, which only reads the name.
  [[nodiscard]] std::optional<std::string>
  getName(const AstNode &node) const override;
};

} // namespace pegium::references
