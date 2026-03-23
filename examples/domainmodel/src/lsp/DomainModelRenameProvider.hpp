#pragma once

#include <optional>
#include <string_view>

#include <domainmodel/services/Services.hpp>

#include <pegium/lsp/navigation/DefaultRenameProvider.hpp>

namespace domainmodel::services::lsp {

class DomainModelRenameProvider final : public pegium::DefaultRenameProvider {
public:
  using pegium::DefaultRenameProvider::DefaultRenameProvider;

  std::optional<::lsp::WorkspaceEdit>
  rename(const pegium::workspace::Document &document,
         const ::lsp::RenameParams &params,
         const pegium::utils::CancellationToken &cancelToken) const override;

private:
  [[nodiscard]] std::optional<std::string>
  buildQualifiedName(const pegium::AstNode &node, const pegium::AstNode &renamedRoot,
                     std::string_view replacementName,
                     const references::QualifiedNameProvider
                         *qualifiedNameProvider) const;
};

} // namespace domainmodel::services::lsp
