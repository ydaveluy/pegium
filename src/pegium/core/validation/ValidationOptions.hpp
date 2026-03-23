#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace pegium::validation {

/// Validation behavior switches used when checking one document.
struct ValidationOptions {
  std::vector<std::string> categories;
  std::optional<bool> stopAfterParsingErrors;
  std::optional<bool> stopAfterLinkingErrors;

  [[nodiscard]] bool empty() const noexcept { return categories.empty(); }
};

/// Build-time validation setting: disabled, enabled, or detailed options.
using BuildValidationOption =
    std::variant<std::monostate, bool, ValidationOptions>;

/// Returns whether validation is enabled for a build option.
[[nodiscard]] inline bool
is_validation_enabled(const BuildValidationOption &option) noexcept {
  if (const auto *enabled = std::get_if<bool>(&option)) {
    return *enabled;
  }
  return std::holds_alternative<ValidationOptions>(option);
}

/// Returns the detailed validation options carried by `option`, when any.
[[nodiscard]] inline const ValidationOptions *
get_validation_options(const BuildValidationOption &option) noexcept {
  return std::get_if<ValidationOptions>(&option);
}

} // namespace pegium::validation
