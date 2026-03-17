#pragma once

#include <optional>
#include <string>

#include <pegium/workspace/AstDescriptions.hpp>

namespace pegium {
struct AstNode;
}

namespace pegium::workspace {

struct Document;

class AstNodeDescriptionProvider {
public:
  virtual ~AstNodeDescriptionProvider() noexcept = default;

  [[nodiscard]] virtual std::optional<AstNodeDescription>
  createDescription(const AstNode &node, const Document &document,
                    std::string name = {}) const = 0;
};

} // namespace pegium::workspace
