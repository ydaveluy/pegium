#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <pegium/core/services/Diagnostic.hpp>
#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/validation/ValidationOptions.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::validation {

class ValidationRegistry;

/// Validates one fully managed document and produces diagnostics.
class DocumentValidator {
public:
  virtual ~DocumentValidator() noexcept = default;
  /// Returns the diagnostics produced for `document` under `options`.
  [[nodiscard]] virtual std::vector<pegium::Diagnostic>
  validateDocument(const workspace::Document &document,
                   const ValidationOptions &options,
                   const utils::CancellationToken &cancelToken) const = 0;
};

} // namespace pegium::validation
