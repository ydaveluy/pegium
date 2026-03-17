#pragma once

#include <memory>
#include <vector>

#include <domainmodel/ast.hpp>

#include <pegium/references/DefaultScopeComputation.hpp>

#include "QualifiedNameProvider.hpp"

namespace domainmodel::services::references {

class DomainModelScopeComputation final
    : public pegium::references::DefaultScopeComputation {
public:
  DomainModelScopeComputation(
      const pegium::services::CoreServices &services,
      std::shared_ptr<const QualifiedNameProvider> qualifiedNameProvider);

  std::vector<pegium::workspace::AstNodeDescription> collectExportedSymbols(
      const pegium::workspace::Document &document,
      const pegium::utils::CancellationToken &cancelToken) const override;

  pegium::workspace::LocalSymbols collectLocalSymbols(
      const pegium::workspace::Document &document,
      const pegium::utils::CancellationToken &cancelToken) const override;

private:
  std::vector<pegium::workspace::AstNodeDescription> processContainer(
      const ast::DomainModel &container,
      const pegium::workspace::Document &document,
      pegium::workspace::LocalSymbols &symbols,
      const pegium::utils::CancellationToken &cancelToken) const;

  std::vector<pegium::workspace::AstNodeDescription> processContainer(
      const ast::PackageDeclaration &container,
      const pegium::workspace::Document &document,
      pegium::workspace::LocalSymbols &symbols,
      const pegium::utils::CancellationToken &cancelToken) const;

  void collectExportedSymbols(
      const std::vector<pegium::AstNode::pointer<ast::AbstractElement>> &elements,
      std::string_view qualifier, const pegium::workspace::Document &document,
      std::vector<pegium::workspace::AstNodeDescription> &symbols,
      const pegium::utils::CancellationToken &cancelToken) const;

  std::shared_ptr<const QualifiedNameProvider> _qualifiedNameProvider;
};

} // namespace domainmodel::services::references
