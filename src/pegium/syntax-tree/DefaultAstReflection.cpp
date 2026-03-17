#include <pegium/syntax-tree/DefaultAstReflection.hpp>

namespace pegium {

namespace {

std::type_index invalid_type() noexcept {
  return std::type_index(typeid(void));
}

} // namespace

std::vector<std::type_index> DefaultAstReflection::getAllTypes() const {
  std::scoped_lock lock(_mutex);

  std::vector<std::type_index> types;
  types.reserve(_types.size());
  for (const auto &type : _types) {
    types.push_back(type);
  }
  return types;
}

std::optional<bool>
DefaultAstReflection::lookupSubtype(std::type_index subtype,
                                    std::type_index supertype) const {
  std::scoped_lock lock(_mutex);
  if (subtype == supertype) {
    return true;
  }
  if (subtype == invalid_type() || supertype == invalid_type()) {
    return false;
  }
  const auto relation = KnownRelationKey{.subtype = subtype,
                                         .supertype = supertype};
  if (const auto it = _knownRelations.find(relation);
      it != _knownRelations.end()) {
    return it->second;
  }
  return std::nullopt;
}

bool DefaultAstReflection::isSubtype(std::type_index subtype,
                                     std::type_index supertype) const {
  return lookupSubtype(subtype, supertype).value_or(false);
}

void DefaultAstReflection::registerSubtype(std::type_index subtype,
                                           std::type_index supertype) const {
  std::scoped_lock lock(_mutex);
  registerTypeLocked(subtype);
  registerTypeLocked(supertype);
  std::vector<std::type_index> allSubtypes{subtype};
  if (const auto it = _knownSubtypesReverse.find(subtype);
      it != _knownSubtypesReverse.end()) {
    allSubtypes.insert(allSubtypes.end(), it->second.begin(), it->second.end());
  }

  std::vector<std::type_index> allSupertypes{supertype};
  if (const auto it = _knownSubtypes.find(supertype);
      it != _knownSubtypes.end()) {
    allSupertypes.insert(allSupertypes.end(), it->second.begin(), it->second.end());
  }

  for (const auto &knownSubtype : allSubtypes) {
    auto &knownSupertypes = _knownSubtypes[knownSubtype];
    auto &knownNonSubtypes = _knownNonSubtypes[knownSubtype];
    for (const auto &knownSupertype : allSupertypes) {
      if (knownSubtype == knownSupertype) {
        continue;
      }
      _knownRelations[KnownRelationKey{.subtype = knownSubtype,
                                       .supertype = knownSupertype}] = true;
      knownNonSubtypes.erase(knownSupertype);
      knownSupertypes.insert(knownSupertype);
      _knownSubtypesReverse[knownSupertype].insert(knownSubtype);
    }
  }
}

void DefaultAstReflection::registerNonSubtype(std::type_index subtype,
                                              std::type_index supertype) const {
  std::scoped_lock lock(_mutex);
  registerTypeLocked(subtype);
  registerTypeLocked(supertype);
  const auto relation = KnownRelationKey{.subtype = subtype,
                                         .supertype = supertype};
  if (const auto it = _knownRelations.find(relation);
      it != _knownRelations.end() && it->second) {
    return;
  }
  _knownRelations.insert_or_assign(relation, false);
  _knownNonSubtypes[subtype].insert(supertype);
}

void DefaultAstReflection::registerTypeLocked(std::type_index type) const {
  _types.insert(type);
}

} // namespace pegium
