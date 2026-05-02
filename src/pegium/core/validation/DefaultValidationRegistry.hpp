#pragma once

#include <memory>
#include <span>
#include <string>
#include <typeindex>
#include <vector>

#include <pegium/core/services/DefaultCoreService.hpp>
#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>

namespace pegium {
struct AstNode;
} // namespace pegium

namespace pegium::validation {

class ValidationAcceptor;

namespace detail {
struct CompiledValidationRegistry;
} // namespace detail

/// Default mutable registry of validation checks and preparations.
///
/// Adds no public API beyond the abstract `ValidationRegistry`: every
/// implementation detail (the compiled registry snapshot, per-call category
/// mask caching) is private. Tests interact through the abstract interface.
class DefaultValidationRegistry : public ValidationRegistry,
                                  protected pegium::DefaultCoreService {
public:
  using ValidationRegistry::registerCheck;
  using pegium::DefaultCoreService::DefaultCoreService;

  // ValidationRegistry overrides:
  [[nodiscard]] std::vector<std::string>
  getAllValidationCategories() const override;
  void registerBeforeDocument(ValidationPreparation check) override;
  void registerAfterDocument(ValidationPreparation check) override;
  [[nodiscard]] std::span<const ValidationPreparation>
  checksBefore() const noexcept override;
  [[nodiscard]] std::span<const ValidationPreparation>
  checksAfter() const noexcept override;
  void runChecks(const AstNode &node, const ValidationAcceptor &acceptor,
                 std::span<const std::string> categories,
                 const utils::CancellationToken &cancelToken) const override;

private:
  struct RegisteredValidationCheckEntry {
    std::type_index targetType = std::type_index(typeid(AstNode));
    ValidationCheck check;
    std::string category;
  };

  static void validate_category(std::string_view category);
  [[nodiscard]] std::shared_ptr<const detail::CompiledValidationRegistry>
  compiledRegistry() const;

  /// Returns a per-category bitmask sized to `compiled.knownCategories`, with
  /// identity-based caching across consecutive calls with the same `categories`
  /// span pointer/size. The cache is invalidated when the compiled snapshot
  /// changes.
  [[nodiscard]] const std::vector<bool> &
  enabled_category_mask(const detail::CompiledValidationRegistry &compiled,
                        std::span<const std::string> categories) const;

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

  // Identity-based cache for the latest (compiled, categories) pair.
  mutable const detail::CompiledValidationRegistry *_cachedMaskCompiled =
      nullptr;
  mutable const std::string *_cachedMaskCategoriesData = nullptr;
  mutable std::size_t _cachedMaskCategoriesSize = 0;
  mutable std::vector<bool> _cachedMask;
};

} // namespace pegium::validation
