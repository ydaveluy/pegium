#include <pegium/core/syntax-tree/DefaultAstReflection.hpp>

#include <typeindex>
#include <vector>

#include <pegium/core/syntax-tree/AstNode.hpp>

namespace pegium {
namespace {

[[nodiscard]] std::type_index invalid_type() noexcept {
  return std::type_index(typeid(void));
}

[[nodiscard]] bool is_valid_type(std::type_index type) noexcept {
  return type != invalid_type();
}

[[nodiscard]] std::type_index ast_node_type() noexcept {
  return std::type_index(typeid(AstNode));
}

[[nodiscard]] const std::unordered_set<std::type_index> &
empty_type_set() noexcept {
  static const std::unordered_set<std::type_index> empty;
  return empty;
}

} // namespace

const std::unordered_set<std::type_index> &
DefaultAstReflection::getAllTypes() const {
  return _types;
}

bool DefaultAstReflection::isInstance(const AstNode &node,
                                      std::type_index type) const {
  const auto actualType = std::type_index(typeid(node));
  return isSubtype(actualType, type);
}

bool DefaultAstReflection::isSubtype(std::type_index subtype,
                                     std::type_index supertype) const {
  if (subtype == supertype) {
    return true;
  }
  if (const auto it = _supertypesByType.find(subtype);
      it != _supertypesByType.end()) {
    return it->second.contains(supertype);
  }
  return false;
}

const std::unordered_set<std::type_index> &
DefaultAstReflection::getAllSubTypes(std::type_index type) const {
  if (type == invalid_type() || !_types.contains(type)) {
    return empty_type_set();
  }
  return _subtypesByType.at(type);
}

void DefaultAstReflection::registerType(std::type_index type) {
  if (type == invalid_type()) {
    return;
  }
  if (type == ast_node_type()) {
    registerTypeInternal(type);
    return;
  }
  registerSubtype(type, ast_node_type());
}

void DefaultAstReflection::registerSubtype(std::type_index subtype,
                                           std::type_index supertype) {
  if (subtype == invalid_type() || supertype == invalid_type() ||
      subtype == supertype) {
    registerTypeInternal(subtype);
    registerTypeInternal(supertype);
    return;
  }

  registerTypeInternal(subtype);
  registerTypeInternal(supertype);

  const auto &allSubtypes = _subtypesByType.at(subtype);

  std::vector<std::type_index> allSupertypes{supertype};
  if (const auto it = _supertypesByType.find(supertype);
      it != _supertypesByType.end()) {
    allSupertypes.insert(allSupertypes.end(), it->second.begin(),
                         it->second.end());
  }

  for (const auto knownSubtype : allSubtypes) {
    auto &supertypes = _supertypesByType[knownSubtype];
    for (const auto knownSupertype : allSupertypes) {
      if (knownSubtype == knownSupertype) {
        continue;
      }
      if (supertypes.insert(knownSupertype).second) {
        _subtypesByType[knownSupertype].insert(knownSubtype);
      }
    }
  }
}

void DefaultAstReflection::registerTypeInternal(std::type_index type) {
  if (!is_valid_type(type)) {
    return;
  }
  if (_types.insert(type).second) {
    _subtypesByType[type].insert(type);
    return;
  }
  _subtypesByType[type].insert(type);
}

} // namespace pegium
