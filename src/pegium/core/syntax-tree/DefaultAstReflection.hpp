#pragma once

/// Default mutable AST reflection implementation used by core services.

#include <typeindex>
#include <unordered_map>
#include <unordered_set>

#include <pegium/core/syntax-tree/AstReflection.hpp>

namespace pegium {

class DefaultAstReflection final : public AstReflection {
public:
  DefaultAstReflection() = default;

  [[nodiscard]] const std::unordered_set<std::type_index> &
  getAllTypes() const override;
  void registerType(std::type_index type) override;
  void registerSubtype(std::type_index subtype,
                       std::type_index supertype) override;
  [[nodiscard]] bool isInstance(const AstNode &node,
                                std::type_index type) const override;
  [[nodiscard]] bool isSubtype(std::type_index subtype,
                               std::type_index supertype) const override;
  [[nodiscard]] const std::unordered_set<std::type_index> &
  getAllSubTypes(std::type_index type) const override;

private:
  void registerTypeInternal(std::type_index type);

  std::unordered_set<std::type_index> _types;
  std::unordered_map<std::type_index, std::unordered_set<std::type_index>>
      _supertypesByType;
  std::unordered_map<std::type_index, std::unordered_set<std::type_index>>
      _subtypesByType;
};

} // namespace pegium
