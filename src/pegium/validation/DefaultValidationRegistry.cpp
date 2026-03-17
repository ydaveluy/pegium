#include <pegium/validation/DefaultValidationRegistry.hpp>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <pegium/validation/DiagnosticRanges.hpp>

namespace pegium::validation {

namespace {

template <typename Check>
void run_check_with_validation_exception_handling(
    const Check &check, const AstNode &node,
    const ValidationAcceptor &acceptor) {
    try {
      check(node, acceptor);
    } catch (const utils::OperationCancelled &) {
      throw;
    } catch (const std::exception &error) {
      const auto message =
          std::string("An error occurred during validation: ") + error.what();
      acceptor.error(node, message);
    } catch (...) {
      const std::string message = "An error occurred during validation.";
      acceptor.error(node, message);
    }
}

ValidationCheck wrap_validation_exception(ValidationCheck check) {
  return [check = std::move(check)](const AstNode &node,
                                    const ValidationAcceptor &acceptor) {
    run_check_with_validation_exception_handling(check, node, acceptor);
  };
}

ValidationPreparation
wrap_preparation_exception(ValidationPreparation check,
                           std::string_view messageContext) {
  return [check = std::move(check), messageContext = std::string(messageContext)](
             const AstNode &rootNode, const ValidationAcceptor &acceptor,
             std::span<const std::string> categories,
             const utils::CancellationToken &cancelToken) {
    try {
      check(rootNode, acceptor, categories, cancelToken);
    } catch (const utils::OperationCancelled &) {
      throw;
    } catch (const std::exception &error) {
      const auto message = messageContext + ": " + error.what();
      acceptor.error(rootNode, message);
    } catch (...) {
      acceptor.error(rootNode, messageContext);
    }
  };
}

} // namespace

bool DefaultValidationRegistry::includes_category(
    std::span<const std::string> categories,
    std::string_view category) noexcept {
  return std::ranges::find(categories, category) != categories.end();
}

void DefaultValidationRegistry::validate_category(std::string_view category) {
  if (category == kBuiltInValidationCategory) {
    throw std::invalid_argument(
        "The 'built-in' category is reserved for parser/linking diagnostics.");
  }
}

void DefaultValidationRegistry::registerMatcher(NodeMatcher matcher,
                                                ValidationCheck check,
                                                MatchedValidationCheck matchedCheck,
                                                std::string_view category) {
  if (!matcher || !check || !matchedCheck || category.empty()) {
    return;
  }
  validate_category(category);

  const std::string storedCategory(category);
  std::scoped_lock lock(_mutex);
  _checks.push_back(std::make_unique<ValidationCheckEntry>(
      ValidationCheckEntry{.matcher = std::move(matcher),
                           .check = wrap_validation_exception(std::move(check)),
                           .matchedCheck = std::move(matchedCheck),
                           .category = storedCategory}));
  _checkEntriesByType.clear();
  _revision.fetch_add(1, std::memory_order_relaxed);
  _knownCategories.insert(storedCategory);
}

void DefaultValidationRegistry::registerCheck(ValidationCheck check,
                                              std::string_view category) {
  registerMatcher(
      [](const AstNode &) { return true; }, check, std::move(check), category);
}

void DefaultValidationRegistry::registerCheck(std::type_index nodeType,
                                              ValidationCheck check,
                                              std::string_view category) {
  if (!check) {
    return;
  }

  registerMatcher(
      [nodeType](const AstNode &node) {
        return std::type_index(typeid(node)) == nodeType;
      },
      check, std::move(check), category);
}

std::shared_ptr<const DefaultValidationRegistry::CachedCheckEntries>
DefaultValidationRegistry::resolveCheckEntries(const AstNode &node) const {
  const auto nodeType = std::type_index(typeid(node));

  std::scoped_lock lock(_mutex);
  if (const auto cacheIt = _checkEntriesByType.find(nodeType);
      cacheIt != _checkEntriesByType.end()) {
    return cacheIt->second;
  }

  auto entries = std::make_shared<CachedCheckEntries>();
  entries->reserve(_checks.size());
  for (const auto &entry : _checks) {
    if (entry->matcher(node)) {
      entries->push_back(entry.get());
    }
  }

  std::shared_ptr<const CachedCheckEntries> result = entries;
  _checkEntriesByType.emplace(nodeType, result);
  return result;
}

utils::stream<ValidationCheck>
DefaultValidationRegistry::getChecks(
    const AstNode &node, std::span<const std::string> categories) const {
  std::vector<ValidationCheck> checks;
  const auto entries = resolveCheckEntries(node);
  checks.reserve(entries->size());
  for (const auto *entry : *entries) {
    if (!categories.empty() &&
        !includes_category(categories, entry->category)) {
      continue;
    }
    checks.push_back(entry->check);
  }

  return utils::make_stream<ValidationCheck>(std::move(checks));
}

void DefaultValidationRegistry::runChecks(
    const AstNode &node, std::span<const std::string> categories,
    const ValidationAcceptor &acceptor) const {
  const auto entries = resolveCheckEntries(node);
  for (const auto *entry : *entries) {
    if (!categories.empty() &&
        !includes_category(categories, entry->category)) {
      continue;
    }
    run_check_with_validation_exception_handling(entry->matchedCheck, node,
                                                 acceptor);
  }
}

std::vector<std::string>
DefaultValidationRegistry::getAllValidationCategories() const {
  std::vector<std::string> categories;

  std::scoped_lock lock(_mutex);
  categories.reserve(_knownCategories.size());
  for (const auto &category : _knownCategories) {
    categories.push_back(category);
  }

  std::ranges::sort(categories);
  return categories;
}

void DefaultValidationRegistry::registerBeforeDocument(
    ValidationPreparation check) {
  if (!check) {
    return;
  }
  std::scoped_lock lock(_mutex);
  _checksBefore.push_back(wrap_preparation_exception(
      std::move(check), "An error occurred during set-up of the validation"));
}

void DefaultValidationRegistry::registerAfterDocument(
    ValidationPreparation check) {
  if (!check) {
    return;
  }
  std::scoped_lock lock(_mutex);
  _checksAfter.push_back(wrap_preparation_exception(
      std::move(check), "An error occurred during tear-down of the validation"));
}

std::span<const ValidationPreparation>
DefaultValidationRegistry::checksBefore() const noexcept {
  return _checksBefore;
}

std::span<const ValidationPreparation>
DefaultValidationRegistry::checksAfter() const noexcept {
  return _checksAfter;
}

void DefaultValidationRegistry::clear() {
  std::scoped_lock lock(_mutex);
  _checks.clear();
  _checkEntriesByType.clear();
  _revision.fetch_add(1, std::memory_order_relaxed);
  _checksBefore.clear();
  _checksAfter.clear();
  _knownCategories = {std::string(kFastValidationCategory),
                      std::string(kSlowValidationCategory),
                      std::string(kBuiltInValidationCategory)};
}

} // namespace pegium::validation
