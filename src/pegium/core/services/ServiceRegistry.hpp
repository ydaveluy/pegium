#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include <pegium/core/services/CoreServices.hpp>

namespace pegium {

/// Resolves the language service container associated with a document URI.
class ServiceRegistry {
public:
  virtual ~ServiceRegistry() noexcept = default;

  /// Registers the services of one language in the shared registry.
  ///
  /// If a later registration claims the same file name or file extension as an
  /// existing language, the later registration replaces that mapping.
  virtual void registerServices(std::unique_ptr<CoreServices> services) = 0;

  /// Resolves the services for a document URI.
  ///
  /// Resolution is URI-first and follows this precedence:
  /// 1. the languageId of an already opened text document for that URI, when
  ///    it matches a registered language
  /// 2. an exact file-name mapping
  /// 3. a file-extension mapping
  ///
  /// Throws when no language can be resolved for the URI.
  [[nodiscard]] virtual const CoreServices &
  getServices(std::string_view uri) const = 0;

  /// Resolves the services for a document URI without throwing.
  ///
  /// Returns `nullptr` when no registered language matches `uri`.
  [[nodiscard]] virtual const CoreServices *
  findServices(std::string_view uri) const noexcept = 0;
  [[nodiscard]] virtual std::vector<const CoreServices *> all() const = 0;
};

} // namespace pegium
