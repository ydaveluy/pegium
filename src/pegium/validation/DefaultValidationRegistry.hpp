#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <pegium/validation/ValidationRegistry.hpp>

namespace pegium::validation {

class DefaultValidationRegistry : public ValidationRegistry {
public:
  using ValidationRegistry::registerCheck;

  void registerCheck(
      ValidationCheck check,
      std::string_view category = kFastValidationCategory) override;
  void registerCheck(
      std::type_index nodeType, ValidationCheck check,
      std::string_view category = kFastValidationCategory) override;

  [[nodiscard]] utils::stream<ValidationCheck>
  getChecks(const AstNode &node,
            std::span<const std::string> categories = {}) const override;
  void runChecks(const AstNode &node, std::span<const std::string> categories,
                 const ValidationAcceptor &acceptor) const override;

  [[nodiscard]] std::vector<std::string>
  getAllValidationCategories() const override;

  void registerBeforeDocument(ValidationPreparation check) override;
  void registerAfterDocument(ValidationPreparation check) override;

  [[nodiscard]] std::span<const ValidationPreparation>
  checksBefore() const noexcept override;
  [[nodiscard]] std::span<const ValidationPreparation>
  checksAfter() const noexcept override;

  void clear() override;

private:
  struct ValidationCheckEntry {
    NodeMatcher matcher;
    ValidationCheck check;
    MatchedValidationCheck matchedCheck;
    std::string category;
  };
  using CachedCheckEntries = std::vector<const ValidationCheckEntry *>;

  static bool includes_category(std::span<const std::string> categories,
                                std::string_view category) noexcept;
  static void validate_category(std::string_view category);
  [[nodiscard]] std::shared_ptr<const CachedCheckEntries>
  resolveCheckEntries(const AstNode &node) const;

  void registerMatcher(NodeMatcher matcher, ValidationCheck check,
                       MatchedValidationCheck matchedCheck,
                       std::string_view category) override;

  mutable std::mutex _mutex;
  std::vector<std::unique_ptr<ValidationCheckEntry>> _checks;
  mutable std::unordered_map<std::type_index,
                             std::shared_ptr<const CachedCheckEntries>>
      _checkEntriesByType;
  mutable std::atomic<std::uint64_t> _revision{0};
  std::vector<ValidationPreparation> _checksBefore;
  std::vector<ValidationPreparation> _checksAfter;
  std::unordered_set<std::string> _knownCategories{
      std::string(kFastValidationCategory),
      std::string(kSlowValidationCategory),
      std::string(kBuiltInValidationCategory)};
};

} // namespace pegium::validation
