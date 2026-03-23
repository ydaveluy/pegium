#include <pegium/core/validation/DefaultValidationRegistry.hpp>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <pegium/core/observability/ObservabilitySink.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/syntax-tree/AstUtils.hpp>
#include <pegium/core/validation/DiagnosticRanges.hpp>

namespace pegium::validation {

namespace detail {

struct CompiledValidationCheckEntry {
  std::type_index targetType = std::type_index(typeid(AstNode));
  ValidationCheck check;
  std::string category;
  std::size_t categoryId = 0;
};

using CompiledValidationCheckList =
    std::vector<const CompiledValidationCheckEntry *>;
using CompiledValidationCheckIndex =
    std::unordered_map<std::type_index, CompiledValidationCheckList>;

struct CompiledValidationRegistry {
  std::vector<std::string> knownCategories;
  std::unordered_map<std::string, std::size_t> categoryIdsByName;
  std::vector<CompiledValidationCheckEntry> checks;
  CompiledValidationCheckIndex checksByType;
  std::vector<ValidationPreparation> checksBefore;
  std::vector<ValidationPreparation> checksAfter;
};

} // namespace detail

namespace {

void publish_validation_observation(
    observability::ObservabilitySink &sink, const AstNode &node,
    observability::ObservationCode code, std::string message,
    std::string category = {}) {
  observability::Observation observation{
      .severity = observability::ObservationSeverity::Error,
      .code = code,
      .message = std::move(message),
      .category = std::move(category)};
  if (const auto *document = tryGetDocument(node); document != nullptr) {
    observation.uri = document->uri;
    observation.languageId = document->textDocument().languageId();
    observation.documentId = document->id;
    observation.state = document->state;
  }
  sink.publish(observation);
}

[[nodiscard]] ValidationCheck wrap_validation_check(
    ValidationCheck check,
    const std::shared_ptr<observability::ObservabilitySink> &sink,
    std::string category) {
  return [check = std::move(check), sink, category = std::move(category)](
             const AstNode &node, const ValidationAcceptor &acceptor,
             const utils::CancellationToken &cancelToken) {
    try {
      check(node, acceptor, cancelToken);
    } catch (const utils::OperationCancelled &) {
      throw;
    } catch (const std::exception &error) {
      auto message = std::string("An error occurred during validation: ") +
                     error.what();
      publish_validation_observation(
          *sink, node, observability::ObservationCode::ValidationCheckThrew,
          message, category);
      acceptor.error(node, message);
    } catch (...) {
      auto message = std::string("An error occurred during validation.");
      publish_validation_observation(
          *sink, node, observability::ObservationCode::ValidationCheckThrew,
          message, category);
      acceptor.error(node, message);
    }
  };
}

[[nodiscard]] ValidationPreparation wrap_validation_preparation(
    ValidationPreparation check,
    const std::shared_ptr<observability::ObservabilitySink> &sink,
    observability::ObservationCode code, std::string errorPrefix,
    std::string unknownErrorMessage) {
  return [check = std::move(check), sink, code,
          errorPrefix = std::move(errorPrefix),
          unknownErrorMessage = std::move(unknownErrorMessage)](
             const AstNode &rootNode, const ValidationAcceptor &acceptor,
             std::span<const std::string> categories,
             const utils::CancellationToken &cancelToken) {
    try {
      check(rootNode, acceptor, categories, cancelToken);
    } catch (const utils::OperationCancelled &) {
      throw;
    } catch (const std::exception &error) {
      auto message = errorPrefix + error.what();
      publish_validation_observation(*sink, rootNode, code, message);
      acceptor.error(rootNode, message);
    } catch (...) {
      publish_validation_observation(*sink, rootNode, code,
                                     unknownErrorMessage);
      acceptor.error(rootNode, unknownErrorMessage);
    }
  };
}

class PreparedChecksImpl final : public ValidationRegistry::PreparedChecks {
public:
  explicit PreparedChecksImpl(
      std::shared_ptr<const detail::CompiledValidationRegistry> compiled)
      : _compiled(std::move(compiled)), _checksByType(&_compiled->checksByType) {
  }

  PreparedChecksImpl(
      std::shared_ptr<const detail::CompiledValidationRegistry> compiled,
      detail::CompiledValidationCheckIndex filteredChecksByType)
      : _compiled(std::move(compiled)),
        _filteredChecksByType(std::move(filteredChecksByType)),
        _checksByType(&_filteredChecksByType) {}

  void run(const AstNode &node, const ValidationAcceptor &acceptor,
           const utils::CancellationToken &cancelToken) const override {
    const auto checksIt = _checksByType->find(std::type_index(typeid(node)));
    if (checksIt == _checksByType->end()) {
      return;
    }

    for (const auto *entry : checksIt->second) {
      utils::throw_if_cancelled(cancelToken);
      entry->check(node, acceptor, cancelToken);
    }
  }

private:
  std::shared_ptr<const detail::CompiledValidationRegistry> _compiled;
  detail::CompiledValidationCheckIndex _filteredChecksByType;
  const detail::CompiledValidationCheckIndex *_checksByType = nullptr;
};

[[nodiscard]] detail::CompiledValidationCheckIndex
build_filtered_check_index(
    const detail::CompiledValidationRegistry &compiled,
    std::span<const std::string> categories) {
  std::vector<bool> enabledCategories(compiled.knownCategories.size(), false);
  std::size_t enabledCategoryCount = 0;
  for (const auto &category : categories) {
    const auto categoryIt = compiled.categoryIdsByName.find(category);
    if (categoryIt == compiled.categoryIdsByName.end()) {
      continue;
    }
    if (!enabledCategories[categoryIt->second]) {
      enabledCategories[categoryIt->second] = true;
      ++enabledCategoryCount;
    }
  }

  if (enabledCategoryCount == 0) {
    return {};
  }

  detail::CompiledValidationCheckIndex filteredChecksByType;
  filteredChecksByType.reserve(compiled.checksByType.size());
  for (const auto &[nodeType, checks] : compiled.checksByType) {
    detail::CompiledValidationCheckList filteredChecks;
    filteredChecks.reserve(checks.size());
    for (const auto *entry : checks) {
      if (enabledCategories[entry->categoryId]) {
        filteredChecks.push_back(entry);
      }
    }
    if (!filteredChecks.empty()) {
      filteredChecksByType.try_emplace(nodeType, std::move(filteredChecks));
    }
  }

  return filteredChecksByType;
}

} // namespace

void DefaultValidationRegistry::validate_category(std::string_view category) {
  if (category == kBuiltInValidationCategory) {
    throw std::invalid_argument(
        "The 'built-in' category is reserved for parser/linking diagnostics.");
  }
}

std::shared_ptr<const detail::CompiledValidationRegistry>
DefaultValidationRegistry::compiledRegistry() const {
  if (!_compiledDirty) {
    assert(_compiled != nullptr);
    return _compiled;
  }

  auto compiled = std::make_shared<detail::CompiledValidationRegistry>();
  compiled->knownCategories = _knownCategories;
  compiled->checksBefore = _registeredChecksBefore;
  compiled->checksAfter = _registeredChecksAfter;

  compiled->categoryIdsByName.reserve(compiled->knownCategories.size());
  for (std::size_t index = 0; index < compiled->knownCategories.size();
       ++index) {
    compiled->categoryIdsByName.try_emplace(compiled->knownCategories[index],
                                            index);
  }

  compiled->checks.reserve(_registeredChecks.size());
  for (const auto &entry : _registeredChecks) {
    const auto categoryIt = compiled->categoryIdsByName.find(entry.category);
    compiled->checks.push_back(detail::CompiledValidationCheckEntry{
        .targetType = entry.targetType,
        .check = entry.check,
        .category = entry.category,
        .categoryId = categoryIt->second});
  }

  const auto &reflection = *services.shared.astReflection;
  for (const auto &entry : compiled->checks) {
    compiled->checksByType[entry.targetType].push_back(&entry);
    for (const auto subtype : reflection.getAllSubTypes(entry.targetType)) {
      if (subtype == entry.targetType) {
        continue;
      }
      compiled->checksByType[subtype].push_back(&entry);
    }
  }

  _compiled = std::move(compiled);
  _compiledDirty = false;

  return _compiled;
}

void DefaultValidationRegistry::registerTypedCheck(
    ValidationCheckRegistration registration, std::string_view category) {
  if (!registration.check || category.empty()) {
    return;
  }

  validate_category(category);

  const std::string storedCategory(category);
  _registeredChecks.push_back(RegisteredValidationCheckEntry{
      .targetType = registration.targetType,
      .check = wrap_validation_check(
          std::move(registration.check), services.shared.observabilitySink,
          storedCategory),
      .category = storedCategory});
  if (std::ranges::find(_knownCategories, storedCategory) ==
      _knownCategories.end()) {
    _knownCategories.push_back(storedCategory);
  }
  _compiledDirty = true;
}

std::unique_ptr<const ValidationRegistry::PreparedChecks>
DefaultValidationRegistry::prepareChecks(
    std::span<const std::string> categories) const {
  auto compiled = compiledRegistry();
  if (categories.empty()) {
    return std::make_unique<PreparedChecksImpl>(std::move(compiled));
  }
  return std::make_unique<PreparedChecksImpl>(
      compiled, build_filtered_check_index(*compiled, categories));
}

std::vector<std::string>
DefaultValidationRegistry::getAllValidationCategories() const {
  return compiledRegistry()->knownCategories;
}

void DefaultValidationRegistry::registerBeforeDocument(
    ValidationPreparation check) {
  if (!check) {
    return;
  }

  _registeredChecksBefore.push_back(wrap_validation_preparation(
      std::move(check), services.shared.observabilitySink,
      observability::ObservationCode::ValidationPreparationThrew,
      "An error occurred during set-up of the validation: ",
      "An error occurred during set-up of the validation"));
  _compiledDirty = true;
}

void DefaultValidationRegistry::registerAfterDocument(
    ValidationPreparation check) {
  if (!check) {
    return;
  }

  _registeredChecksAfter.push_back(wrap_validation_preparation(
      std::move(check), services.shared.observabilitySink,
      observability::ObservationCode::ValidationFinalizationThrew,
      "An error occurred during tear-down of the validation: ",
      "An error occurred during tear-down of the validation"));
  _compiledDirty = true;
}

std::span<const ValidationPreparation>
DefaultValidationRegistry::checksBefore() const noexcept {
  return _registeredChecksBefore;
}

std::span<const ValidationPreparation>
DefaultValidationRegistry::checksAfter() const noexcept {
  return _registeredChecksAfter;
}

} // namespace pegium::validation
