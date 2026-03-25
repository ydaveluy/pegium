#pragma once

#include <memory>
#include <typeindex>
#include <vector>

#include <pegium/core/services/DefaultCoreService.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>

namespace pegium::validation {

namespace detail {
struct CompiledValidationRegistry;
}

/// Default mutable registry of validation checks and preparations.
class DefaultValidationRegistry : public ValidationRegistry,
                                  protected pegium::DefaultCoreService {
public:
  using ValidationRegistry::registerCheck;
  using pegium::DefaultCoreService::DefaultCoreService;

  [[nodiscard]] std::unique_ptr<const PreparedChecks>
  prepareChecks(std::span<const std::string> categories = {}) const override;

  [[nodiscard]] std::vector<std::string>
  getAllValidationCategories() const override;

  void registerBeforeDocument(ValidationPreparation check) override;
  void registerAfterDocument(ValidationPreparation check) override;

  [[nodiscard]] std::span<const ValidationPreparation>
  checksBefore() const noexcept override;
  [[nodiscard]] std::span<const ValidationPreparation>
  checksAfter() const noexcept override;

private:
  struct RegisteredValidationCheckEntry {
    std::type_index targetType = std::type_index(typeid(AstNode));
    ValidationCheck check;
    std::string category;
  };

  static void validate_category(std::string_view category);
  [[nodiscard]] std::shared_ptr<const detail::CompiledValidationRegistry>
  compiledRegistry() const;

  void registerTypedCheck(ValidationCheckRegistration registration,
                          std::string_view category) override;

  mutable std::vector<RegisteredValidationCheckEntry> _registeredChecks;
  mutable std::vector<ValidationPreparation> _registeredChecksBefore;
  mutable std::vector<ValidationPreparation> _registeredChecksAfter;
  mutable std::vector<std::string> _knownCategories{
      std::string(kFastValidationCategory),
      std::string(kSlowValidationCategory),
      std::string(kBuiltInValidationCategory)};
  mutable std::shared_ptr<const detail::CompiledValidationRegistry> _compiled;
  mutable bool _compiledDirty = true;
};

} // namespace pegium::validation
