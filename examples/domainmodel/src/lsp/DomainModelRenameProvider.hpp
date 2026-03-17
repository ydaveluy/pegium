#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include <pegium/lsp/DefaultRenameProvider.hpp>
#include <pegium/workspace/Documents.hpp>
#include <pegium/workspace/IndexManager.hpp>

#include "../references/QualifiedNameProvider.hpp"

namespace domainmodel::services::lsp {

class DomainModelRenameProvider final : public pegium::lsp::DefaultRenameProvider {
public:
  DomainModelRenameProvider(
      const pegium::services::Services &services,
      const pegium::workspace::IndexManager &indexManager,
      const pegium::workspace::Documents &documentStore,
      std::shared_ptr<const references::QualifiedNameProvider>
          qualifiedNameProvider);

  std::optional<::lsp::WorkspaceEdit>
  rename(const pegium::workspace::Document &document,
         const ::lsp::RenameParams &params,
         const pegium::utils::CancellationToken &cancelToken) const override;

private:
  [[nodiscard]] bool isQualifiedReference(
      const pegium::workspace::ReferenceDescription &reference) const;

  const pegium::workspace::IndexManager *_indexManager = nullptr;
  const pegium::workspace::Documents *_documentStore = nullptr;
  std::shared_ptr<const references::QualifiedNameProvider> _qualifiedNameProvider;
};

} // namespace domainmodel::services::lsp
