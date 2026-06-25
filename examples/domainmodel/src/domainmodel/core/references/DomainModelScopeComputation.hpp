#pragma once

#include <vector>

#include <domainmodel/core/Services.hpp>
#include <domainmodel/core/ast.hpp>
#include <domainmodel/core/references/QualifiedNameProvider.hpp>

#include <pegium/core/references/DefaultScopeComputation.hpp>
#include <pegium/core/services/ServiceAccess.hpp>

namespace domainmodel::references {

class DomainModelScopeComputation final
    : public pegium::references::DefaultScopeComputation,
      public pegium::LanguageServiceMixin<DomainModelAddedServices> {
public:
  /// Captures both the Pegium core back-reference (for `services.*`) and the
  /// typed domain-model back-reference (for `languageServices.*`) from the same
  /// container, deducing whether it is the headless or LSP variant.
  template <typename Container>
  explicit DomainModelScopeComputation(const Container &services)
      : pegium::references::DefaultScopeComputation(services),
        pegium::LanguageServiceMixin<DomainModelAddedServices>(services) {}

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

  [[nodiscard]] static pegium::workspace::AstNodeDescription
  createQualifiedDescription(
      const ast::PackageDeclaration &package,
      pegium::workspace::AstNodeDescription description,
      const QualifiedNameProvider *qualifiedNameProvider);
};

} // namespace domainmodel::references
