#pragma once

#include <functional>
#include <string_view>
#include <typeindex>

namespace pegium {
namespace grammar {
struct Assignment;
}

struct AstNode;

/// Lightweight contextual information about one reference slot on an AST node.
struct ReferenceInfo {
  ReferenceInfo(const AstNode *container, std::string_view referenceText,
                const grammar::Assignment &assignment) noexcept
      : container(container), referenceText(referenceText),
        _assignment(assignment) {}

  [[nodiscard]] const grammar::Assignment &getAssignment() const noexcept;
  [[nodiscard]] std::type_index getReferenceType() const noexcept;
  [[nodiscard]] std::string_view getFeature() const noexcept;

  const AstNode *container = nullptr;
  std::string_view referenceText;

private:
  std::reference_wrapper<const grammar::Assignment> _assignment;
};

} // namespace pegium
