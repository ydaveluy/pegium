#pragma once

#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>

#include <pegium/core/utils/TypeIndexHash.hpp>

namespace pegium {

class AstNode;

/// Set of `std::type_index`es using fast pointer-based hash and equality.
using TypeIndexSet =
    std::unordered_set<std::type_index, utils::FastTypeIndexHash,
                       utils::FastTypeIndexEqual>;

/// Runtime registry describing subtype relationships between AST types.
///
/// Reflection uses `std::type_index` rather than generated enum values so that
/// user-defined AST hierarchies and cross-language extensions can participate
/// without a central type table. The extension point is *registration*
/// (`registerType` / `registerSubtype`), not subclassing — so this is a
/// concrete `final` class, keeping the queries on the hot linking / cast path
/// direct (non-virtual) and inlinable.
///
/// Mutation is a bootstrap concern: Pegium populates the registry during
/// single-threaded language registration before documents are processed
/// concurrently. After bootstrap the queries are read-only and safe for
/// concurrent use.
class AstReflection final {
public:
  AstReflection() = default;

  /// Returns every type currently known to the reflection registry.
  ///
  /// The returned reference stays valid until the registry is mutated. Order is
  /// intentionally unspecified.
  [[nodiscard]] const TypeIndexSet &getAllTypes() const noexcept {
    return _types;
  }

  /// Registers one AST type in the reflection registry.
  void registerType(std::type_index type);

  /// Registers one direct subtype edge in the reflection registry.
  void registerSubtype(std::type_index subtype, std::type_index supertype);

  /// Returns whether `node` is an instance of `type`.
  [[nodiscard]] bool isInstance(const AstNode &node,
                                std::type_index type) const noexcept;

  /// Returns whether `subtype` is known to be a subtype of `supertype`.
  ///
  /// `pegium::AstNode` is treated as the implicit root supertype of every
  /// registered AST type.
  [[nodiscard]] bool isSubtype(std::type_index subtype,
                               std::type_index supertype) const noexcept {
    // `type_index::operator==` falls back to `strcmp` on libstdc++; comparing
    // the `name()` pointers is correct within a single DSO and one instruction.
    if (utils::FastTypeIndexEqual{}(subtype, supertype)) {
      return true;
    }
    if (const auto it = _supertypesByType.find(subtype);
        it != _supertypesByType.end()) {
      return it->second.contains(supertype);
    }
    return false;
  }

  /// Returns whether `type` was registered at all. A type the grammar never
  /// names (rule result / vector element / reference target) — typically an
  /// abstract C++ base — is absent here, so `isSubtype` cannot answer for it
  /// and callers fall back to `dynamic_cast`.
  [[nodiscard]] bool isKnown(std::type_index type) const noexcept {
    return _types.contains(type);
  }

  /// Returns `type` and every currently known subtype of `type`.
  ///
  /// Asking for `pegium::AstNode` returns the registered AST root type plus all
  /// currently known AST subtypes. The returned reference stays valid until the
  /// registry is mutated. Order is intentionally unspecified.
  [[nodiscard]] const TypeIndexSet &
  getAllSubTypes(std::type_index type) const noexcept {
    if (type == std::type_index(typeid(void)) || !_types.contains(type)) {
      return emptyTypeSet();
    }
    if (const auto it = _subtypesByType.find(type);
        it != _subtypesByType.end()) {
      return it->second;
    }
    return emptyTypeSet();
  }

private:
  [[nodiscard]] static const TypeIndexSet &emptyTypeSet() noexcept {
    static const TypeIndexSet empty;
    return empty;
  }

  void registerTypeInternal(std::type_index type);

  TypeIndexSet _types;
  std::unordered_map<std::type_index, TypeIndexSet, utils::FastTypeIndexHash,
                     utils::FastTypeIndexEqual>
      _supertypesByType;
  std::unordered_map<std::type_index, TypeIndexSet, utils::FastTypeIndexHash,
                     utils::FastTypeIndexEqual>
      _subtypesByType;
};

/// True iff a value of type `candidate` may stand where `expected` is required:
/// the same type, or a registered subtype. The `void` type index is the "no
/// type" sentinel and never matches. This is the single gate guarding the
/// `static_cast<Reference<T>>` soundness invariant, shared by the scope
/// provider and the workspace index so the two never drift apart.
[[nodiscard]] inline bool
type_is_assignable(std::type_index candidate, std::type_index expected,
                   const AstReflection &reflection) noexcept {
  // `type_index::operator==` falls back to `strcmp` on libstdc++; the name()
  // pointer compare (FastTypeIndexEqual) is correct within a single DSO and one
  // instruction. This gate runs once per scope bucket per reference, so the
  // strcmp fallback was a measurable slice of the build's __strcmp_avx2 cost.
  static const std::type_index voidType{typeid(void)};
  constexpr utils::FastTypeIndexEqual eq{};
  if (eq(expected, voidType) || eq(candidate, voidType)) {
    return false;
  }
  if (eq(candidate, expected)) {
    return true;
  }
  return reflection.isSubtype(candidate, expected);
}

} // namespace pegium
