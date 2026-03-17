#pragma once

#include <optional>
#include <typeindex>
#include <vector>

namespace pegium {

/// Runtime registry describing subtype relationships between AST types.
///
/// Reflection services use `std::type_index` rather than generated enum values
/// so that user-defined AST hierarchies and cross-language extensions can
/// participate without a central type table.
class AstReflection {
public:
  virtual ~AstReflection() noexcept = default;

  /// Returns every type currently known to the reflection registry.
  [[nodiscard]] virtual std::vector<std::type_index> getAllTypes() const = 0;

  /// Returns a cached subtype answer when already known.
  ///
  /// `std::nullopt` means the relation has not been learned yet.
  [[nodiscard]] virtual std::optional<bool>
  lookupSubtype(std::type_index subtype,
                std::type_index supertype) const = 0;

  /// Returns whether `subtype` is known to be a subtype of `supertype`.
  ///
  /// Implementations may answer `false` either because the relation is known to
  /// be false or because it is currently unknown.
  [[nodiscard]] virtual bool isSubtype(std::type_index subtype,
                                       std::type_index supertype) const = 0;

  /// Registers `subtype <: supertype`.
  ///
  /// Implementations are expected to keep transitive subtype information
  /// coherent after registration.
  virtual void registerSubtype(std::type_index subtype,
                               std::type_index supertype) const = 0;

  /// Registers that `subtype` is not a subtype of `supertype`.
  virtual void registerNonSubtype(std::type_index subtype,
                                  std::type_index supertype) const = 0;
};

} // namespace pegium
