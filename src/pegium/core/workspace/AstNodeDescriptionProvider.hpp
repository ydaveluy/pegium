#pragma once

#include <optional>
#include <string>

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

  /// Creates a resolvable symbol description for `node`.
  ///
  /// Implementations must either return `std::nullopt` or a complete
  /// description with a non-empty name, a valid `documentId`, and a valid
  /// `symbolId` that can later be resolved back to the AST node. Complete
  /// descriptions also carry a non-zero `nameLength` spanning the user-visible
  /// name in source text.
  [[nodiscard]] virtual std::optional<AstNodeDescription>
  createDescription(const AstNode &node, const Document &document,
                    std::string name) const = 0;
};

} // namespace pegium::workspace
