#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <pegium/services/Diagnostic.hpp>
#include <pegium/utils/Cancellation.hpp>
#include <pegium/validation/ValidationOptions.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::validation {

class ValidationRegistry;

class DocumentValidator {
public:
  virtual ~DocumentValidator() noexcept = default;
  [[nodiscard]] virtual std::vector<services::Diagnostic>
  validateDocument(const workspace::Document &document,
                   const ValidationOptions &options = {}) const = 0;
  [[nodiscard]] virtual std::vector<services::Diagnostic>
  validateDocument(const workspace::Document &document,
                   const ValidationOptions &options,
                   const utils::CancellationToken &cancelToken) const;
};

} // namespace pegium::validation
