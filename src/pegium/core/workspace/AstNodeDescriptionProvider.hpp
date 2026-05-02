#pragma once

#include <optional>

#include <pegium/core/references/NameProvider.hpp>
#include <pegium/core/workspace/AstDescriptions.hpp>

namespace pegium {
struct AstNode;
}

namespace pegium::workspace {

struct Document;

/// Builds stable symbol descriptions for AST nodes.
class AstNodeDescriptionProvider {
public:
  virtual ~AstNodeDescriptionProvider() noexcept = default;

  /// Creates a resolvable symbol description for `node` from a precomputed
  /// `(name, cstNode)` pair. The caller typically obtains `nameInfo` via
  /// `NameProvider::nameOf(node)`, avoiding a redundant CST lookup here.
  ///
  /// Implementations must either return `std::nullopt` or a complete
  /// description with a non-empty name, a valid `documentId`, and a valid
  /// `symbolId` that can later be resolved back to the AST node. Complete
  /// descriptions also carry a non-zero `nameLength` spanning the user-visible
  /// name in source text.
  [[nodiscard]] virtual std::optional<AstNodeDescription>
  createDescription(const AstNode &node, const Document &document,
                    references::AstNodeName nameInfo) const = 0;
};

} // namespace pegium::workspace
