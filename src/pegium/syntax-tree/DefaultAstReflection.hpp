#pragma once

#include <mutex>
#include <optional>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <pegium/syntax-tree/AstReflection.hpp>

namespace pegium {

/// Thread-safe default implementation of `AstReflection`.
///
/// The registry stores both positive and negative subtype knowledge and
/// incrementally maintains transitive subtype closure for the relations it has
/// learned so far.
class DefaultAstReflection final : public AstReflection {
public:
  DefaultAstReflection() = default;

  [[nodiscard]] std::vector<std::type_index> getAllTypes() const override;

  [[nodiscard]] std::optional<bool>
  lookupSubtype(std::type_index subtype,
                std::type_index supertype) const override;

  [[nodiscard]] bool isSubtype(std::type_index subtype,
                               std::type_index supertype) const override;

  void registerSubtype(std::type_index subtype,
                       std::type_index supertype) const override;

  void registerNonSubtype(std::type_index subtype,
                          std::type_index supertype) const override;

private:
  struct KnownRelationKey {
    std::type_index subtype = std::type_index(typeid(void));
    std::type_index supertype = std::type_index(typeid(void));

    [[nodiscard]] bool operator==(const KnownRelationKey &) const noexcept =
        default;
  };

  struct KnownRelationKeyHash {
    [[nodiscard]] std::size_t
    operator()(const KnownRelationKey &key) const noexcept {
      const auto subtypeHash = std::hash<std::type_index>{}(key.subtype);
      const auto supertypeHash = std::hash<std::type_index>{}(key.supertype);
      return subtypeHash ^ (supertypeHash + 0x9e3779b9u + (subtypeHash << 6u) +
                            (subtypeHash >> 2u));
    }
  };

  void registerTypeLocked(std::type_index type) const;

  mutable std::mutex _mutex;
  mutable std::unordered_set<std::type_index> _types;
  mutable std::unordered_map<KnownRelationKey, bool, KnownRelationKeyHash>
      _knownRelations;
  mutable std::unordered_map<std::type_index, std::unordered_set<std::type_index>>
      _knownSubtypes;
  mutable std::unordered_map<std::type_index, std::unordered_set<std::type_index>>
      _knownSubtypesReverse;
  mutable std::unordered_map<std::type_index, std::unordered_set<std::type_index>>
      _knownNonSubtypes;
};

} // namespace pegium
