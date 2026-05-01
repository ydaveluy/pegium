#pragma once

#include <domainmodel/ast.hpp>

#include <string>
#include <string_view>

namespace domainmodel::references {

class QualifiedNameProvider final {
public:
  [[nodiscard]] std::string
  getQualifiedName(const ast::PackageDeclaration &qualifier,
                   std::string_view name) const;

  [[nodiscard]] std::string getQualifiedName(std::string_view qualifier,
                                             std::string_view name) const;
};

} // namespace domainmodel::references
