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

  /// Creates a resolvable symbol description for `node` published under `name`
  /// (which may be a qualified name supplied by the scope computation, not
  /// necessarily `NameProvider::getName(node)`). Returns `std::nullopt` for an
  /// empty `name`.
  [[nodiscard]] virtual std::optional<AstNodeDescription>
  createDescription(const AstNode &node, std::string name,
                    const Document &document) const = 0;
};

} // namespace pegium::workspace
