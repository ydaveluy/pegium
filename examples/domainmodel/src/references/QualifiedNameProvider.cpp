#include "references/QualifiedNameProvider.hpp"

namespace domainmodel::services::references {

std::string QualifiedNameProvider::getQualifiedName(
    const ast::PackageDeclaration &qualifier, std::string_view name) const {
  if (const auto *parent =
          dynamic_cast<const ast::PackageDeclaration *>(qualifier.getContainer());
      parent != nullptr) {
    return getQualifiedName(getQualifiedName(*parent, qualifier.name), name);
  }
  return getQualifiedName(qualifier.name, name);
}

std::string QualifiedNameProvider::getQualifiedName(std::string_view qualifier,
                                                    std::string_view name) const {
  if (qualifier.empty()) {
    return std::string(name);
  }
  return std::string(qualifier) + "." + std::string(name);
}

} // namespace domainmodel::services::references
