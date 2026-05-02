#include <pegium/core/syntax-tree/AstNode.hpp>

#include <pegium/core/syntax-tree/AstArena.hpp>

namespace pegium {

const AstReflection *ast_reflection_of(const AstNode &node) noexcept {
  const auto *arena = node.arena();
  return arena != nullptr ? arena->reflection() : nullptr;
}

} // namespace pegium
