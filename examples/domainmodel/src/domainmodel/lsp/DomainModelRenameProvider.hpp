#pragma once

#include <optional>
#include <string_view>

#include <domainmodel/core/references/QualifiedNameProvider.hpp>
#include <domainmodel/lsp/LspServices.hpp>

#include <pegium/core/services/ServiceAccess.hpp>
#include <pegium/lsp/navigation/DefaultRenameProvider.hpp>

namespace domainmodel {

class DomainModelRenameProvider final
    : public pegium::DefaultRenameProvider,
      public pegium::LanguageServiceMixin<domainmodel::DomainModelAddedServices> {
public:
  /// Captures both the Pegium LSP back-reference (for `services.*`) and the
  /// typed domain-model back-reference (for `languageServices.*`).
  explicit DomainModelRenameProvider(const DomainModelServices &services)
      : pegium::DefaultRenameProvider(services),
        pegium::LanguageServiceMixin<domainmodel::DomainModelAddedServices>(
            services) {}

  std::optional<::lsp::WorkspaceEdit>
  rename(const pegium::workspace::Document &document,
         const ::lsp::RenameParams &params,
         const pegium::utils::CancellationToken &cancelToken) const override;

private:
  [[nodiscard]] std::optional<std::string>
  buildQualifiedName(const pegium::AstNode &node, const pegium::AstNode &renamedRoot,
                     std::string_view replacementName,
                     const domainmodel::references::QualifiedNameProvider
                         *qualifiedNameProvider) const;
};

} // namespace domainmodel
