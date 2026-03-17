#include <pegium/validation/DocumentValidator.hpp>

#include <pegium/utils/Cancellation.hpp>

namespace pegium::validation {

std::vector<services::Diagnostic> DocumentValidator::validateDocument(
    const workspace::Document &document, const ValidationOptions &options,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  auto diagnostics = validateDocument(document, options);
  utils::throw_if_cancelled(cancelToken);
  return diagnostics;
}

} // namespace pegium::validation
