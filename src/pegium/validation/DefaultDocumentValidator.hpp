#pragma once

#include <vector>

#include <pegium/services/DefaultCoreService.hpp>
#include <pegium/validation/DocumentValidator.hpp>

namespace pegium::services {
struct CoreServices;
}

namespace pegium::validation {

class DefaultDocumentValidator : public DocumentValidator,
                                 protected services::DefaultCoreService {
public:
  using services::DefaultCoreService::DefaultCoreService;

  [[nodiscard]] std::vector<services::Diagnostic>
  validateDocument(const workspace::Document &document,
                   const ValidationOptions &options = {}) const override {
    return validateDocument(document, options, {});
  }
  [[nodiscard]] std::vector<services::Diagnostic>
  validateDocument(const workspace::Document &document,
                   const ValidationOptions &options,
                   const utils::CancellationToken &cancelToken) const override;

private:
  [[nodiscard]] bool run_builtin_validation(
      const ValidationOptions &options) const noexcept;
  [[nodiscard]] bool run_custom_validation(
      const ValidationOptions &options) const noexcept;
};

} // namespace pegium::validation
