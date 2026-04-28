#pragma once

/// Default mutable AST reflection implementation used by core services.

#include <typeindex>
#include <unordered_map>

#include <pegium/core/syntax-tree/AstReflection.hpp>
#include <pegium/core/utils/TypeIndexHash.hpp>

namespace pegium {

class DefaultAstReflection final : public AstReflection {
public:
  DefaultAstReflection() = default;

  [[nodiscard]] const TypeIndexSet &getAllTypes() const override;
  void registerType(std::type_index type) override;
  void registerSubtype(std::type_index subtype,
                       std::type_index supertype) override;
  [[nodiscard]] bool isInstance(const AstNode &node,
                                std::type_index type) const override;
  [[nodiscard]] bool isSubtype(std::type_index subtype,
                               std::type_index supertype) const override;
  [[nodiscard]] const TypeIndexSet &
  getAllSubTypes(std::type_index type) const override;

private:
  void registerTypeInternal(std::type_index type);

  TypeIndexSet _types;
  std::unordered_map<std::type_index, TypeIndexSet, utils::FastTypeIndexHash>
      _supertypesByType;
  std::unordered_map<std::type_index, TypeIndexSet, utils::FastTypeIndexHash>
      _subtypesByType;
};

} // namespace pegium
