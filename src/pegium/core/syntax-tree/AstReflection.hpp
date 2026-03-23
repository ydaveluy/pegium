#pragma once

#include <typeindex>
#include <unordered_set>

namespace pegium {

class AstNode;

/// Runtime registry describing subtype relationships between AST types.
///
/// Reflection services use `std::type_index` rather than generated enum values
/// so that user-defined AST hierarchies and cross-language extensions can
/// participate without a central type table.
///
/// Mutation is a bootstrap concern: Pegium populates the registry during
/// single-threaded language registration before documents are processed
/// concurrently.
class AstReflection {
public:
  virtual ~AstReflection() noexcept = default;

  /// Returns every type currently known to the reflection registry.
  ///
  /// The returned reference stays valid until the registry is mutated. Order is
  /// intentionally unspecified.
  [[nodiscard]] virtual const std::unordered_set<std::type_index> &
  getAllTypes() const = 0;

  /// Registers one AST type in the reflection registry.
  virtual void registerType(std::type_index type) = 0;

  /// Registers one direct subtype edge in the reflection registry.
  virtual void registerSubtype(std::type_index subtype,
                               std::type_index supertype) = 0;

  /// Returns whether `node` is an instance of `type`.
  [[nodiscard]] virtual bool isInstance(const AstNode &node,
                                        std::type_index type) const = 0;

  /// Returns whether `subtype` is known to be a subtype of `supertype`.
  ///
  /// `pegium::AstNode` is treated as the implicit root supertype of every
  /// registered AST type.
  [[nodiscard]] virtual bool isSubtype(std::type_index subtype,
                                       std::type_index supertype) const = 0;

  /// Returns `type` and every currently known subtype of `type`.
  ///
  /// Asking for `pegium::AstNode` returns the registered AST root type plus all
  /// currently known AST subtypes. The returned reference stays valid until
  /// the registry is mutated. Order is intentionally unspecified.
  [[nodiscard]] virtual const std::unordered_set<std::type_index> &
  getAllSubTypes(std::type_index type) const = 0;
};

} // namespace pegium
