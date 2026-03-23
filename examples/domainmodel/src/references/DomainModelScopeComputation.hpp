#pragma once

#include <vector>

#include <domainmodel/ast.hpp>
#include <domainmodel/services/Services.hpp>

#include <pegium/core/references/DefaultScopeComputation.hpp>

namespace domainmodel::services::references {

class DomainModelScopeComputation final
    : public pegium::references::DefaultScopeComputation {
public:
  using pegium::references::DefaultScopeComputation::DefaultScopeComputation;

  std::vector<pegium::workspace::AstNodeDescription>
  collectExportedSymbols(
      const pegium::workspace::Document &document,
      const pegium::utils::CancellationToken &cancelToken) const override;

  pegium::workspace::LocalSymbols collectLocalSymbols(
      const pegium::workspace::Document &document,
      const pegium::utils::CancellationToken &cancelToken) const override;

private:
  std::vector<pegium::workspace::AstNodeDescription> processContainer(
      const pegium::AstNode &container,
      const std::vector<pegium::AstNode::pointer<ast::AbstractElement>> &elements,
      const pegium::workspace::Document &document,
      pegium::workspace::LocalSymbols &symbols,
      const pegium::utils::CancellationToken &cancelToken,
      const QualifiedNameProvider *qualifiedNameProvider) const;

  [[nodiscard]] pegium::workspace::AstNodeDescription createQualifiedDescription(
      const ast::PackageDeclaration &package,
      pegium::workspace::AstNodeDescription description,
      const QualifiedNameProvider *qualifiedNameProvider) const;
};

} // namespace domainmodel::services::references
