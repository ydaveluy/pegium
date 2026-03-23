#include <pegium/core/services/DefaultServiceRegistry.hpp>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <pegium/core/observability/ObservabilitySink.hpp>
#include <pegium/core/parser/AstReflectionBootstrap.hpp>
#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/utils/Errors.hpp>
#include <pegium/core/utils/UriUtils.hpp>

namespace pegium::services {
namespace {

std::string normalize_extension(std::string_view extension) {
  if (extension.empty()) {
    return {};
  }
  std::string normalized(extension);
  if (!normalized.empty() && normalized.front() != '.') {
    normalized.insert(normalized.begin(), '.');
  }
  return normalized;
}

const CoreServices *lookup_by_language_id_locked(
    const utils::TransparentStringMap<std::unique_ptr<CoreServices>>
        &servicesByLanguageId,
    std::string_view languageId) {
  const auto it = servicesByLanguageId.find(languageId);
  return it == servicesByLanguageId.end() ? nullptr : it->second.get();
}

std::string language_error_message(std::string_view extension,
                                   std::string_view languageId) {
  if (languageId.empty()) {
    return "The service registry contains no services for the extension '" +
           std::string(extension) + "'.";
  }
  return "The service registry contains no services for the extension '" +
         std::string(extension) + "' for language '" + std::string(languageId) +
         "'.";
}

void publish_language_mapping_collision(
    observability::ObservabilitySink &sink, std::string_view languageId,
    std::string message) {
  sink.publish(observability::Observation{
      .severity = observability::ObservationSeverity::Warning,
      .code = observability::ObservationCode::LanguageMappingCollision,
      .message = std::move(message),
      .languageId = std::string(languageId)});
}

} // namespace

void DefaultServiceRegistry::registerServices(
    std::unique_ptr<CoreServices> services) {
  if (!services) {
    throw utils::ServiceRegistrationError("Cannot register null core services.");
  }
  if (services->languageMetaData.languageId.empty()) {
    throw utils::ServiceRegistrationError(
        "Cannot register core services without a languageId.");
  }
  if (!services->isComplete()) {
    throw utils::ServiceRegistrationError(
        "Cannot register incomplete core services for language '" +
        services->languageMetaData.languageId + "'.");
  }

  const std::string languageId = services->languageMetaData.languageId;
  std::scoped_lock lock(_mutex);

  if (services->parser != nullptr) {
    // Reflection bootstrap is serialized with service registration so the
    // shared registry is fully populated before the language becomes
    // discoverable through registry lookups.
    parser::bootstrapAstReflection(services->parser->getEntryRule(),
                                   *shared.astReflection);
  }

  auto [it, inserted] = _servicesByLanguageId.try_emplace(languageId);
  if (inserted) {
    _registrationOrder.push_back(languageId);
  } else if (it->second != nullptr) {
    removeLanguageMappingsLocked(languageId, *it->second);
  }

  it->second = std::move(services);
  addLanguageMappingsLocked(languageId, *it->second);
}

const CoreServices &
DefaultServiceRegistry::getServices(std::string_view uri) const {
  std::scoped_lock lock(_mutex);
  if (_servicesByLanguageId.empty()) {
    throw utils::ServiceRegistryError(
        "The service registry is empty. Use `registerServices` to register "
        "the services of a language.");
  }

  const auto normalizedUri = utils::normalize_uri(uri);
  std::string languageId;
  if (const auto *services = findServicesLocked(normalizedUri, &languageId)) {
    return *services;
  }

  const auto path = utils::file_uri_to_path(normalizedUri);
  if (const auto fileName = path.has_value()
                                ? std::filesystem::path(*path).filename().string()
                                : std::filesystem::path(std::string(normalizedUri))
                                      .filename()
                                      .string();
      !fileName.empty()) {
    if (const auto *services = lookupByFileName(fileName)) {
      return *services;
    }
  }

  const auto extension = path.has_value()
                             ? std::filesystem::path(*path).extension().string()
                             : std::filesystem::path(std::string(normalizedUri))
                                   .extension()
                                   .string();
  if (const auto *services = lookupByExtension(extension)) {
    return *services;
  }

  throw utils::ServiceRegistryError(
      language_error_message(extension, languageId));
}

const CoreServices *
DefaultServiceRegistry::findServices(std::string_view uri) const noexcept {
  std::scoped_lock lock(_mutex);
  if (_servicesByLanguageId.empty()) {
    return nullptr;
  }
  return findServicesLocked(utils::normalize_uri(uri));
}

std::vector<const CoreServices *> DefaultServiceRegistry::all() const {
  std::scoped_lock lock(_mutex);
  std::vector<const CoreServices *> services;
  services.reserve(_registrationOrder.size());
  for (const auto &languageId : _registrationOrder) {
    const auto *service =
        lookup_by_language_id_locked(_servicesByLanguageId, languageId);
    services.push_back(service);
  }
  return services;
}

const CoreServices *DefaultServiceRegistry::findServicesLocked(
    std::string_view normalizedUri, std::string *languageId) const {
  if (const auto provider = shared.workspace.textDocuments;
      provider != nullptr) {
    if (auto textDocument = provider->get(normalizedUri);
        textDocument != nullptr && !textDocument->languageId().empty()) {
      if (languageId != nullptr) {
        *languageId = textDocument->languageId();
      }
      if (const auto *services =
              lookup_by_language_id_locked(_servicesByLanguageId,
                                           textDocument->languageId())) {
        return services;
      }
    }
  }

  const auto path = utils::file_uri_to_path(normalizedUri);
  if (const auto fileName = path.has_value()
                                ? std::filesystem::path(*path).filename().string()
                                : std::filesystem::path(std::string(normalizedUri))
                                      .filename()
                                      .string();
      !fileName.empty()) {
    if (const auto *services = lookupByFileName(fileName)) {
      return services;
    }
  }

  const auto extension = path.has_value()
                             ? std::filesystem::path(*path).extension().string()
                             : std::filesystem::path(std::string(normalizedUri))
                                   .extension()
                                   .string();
  return lookupByExtension(extension);
}

const CoreServices *
DefaultServiceRegistry::lookupByExtension(std::string_view extension) const {
  const auto languageIt =
      _languageIdByExtension.find(normalize_extension(extension));
  if (languageIt == _languageIdByExtension.end()) {
    return nullptr;
  }
  return lookup_by_language_id_locked(_servicesByLanguageId, languageIt->second);
}

const CoreServices *
DefaultServiceRegistry::lookupByFileName(std::string_view fileName) const {
  const auto languageIt = _languageIdByFileName.find(fileName);
  if (languageIt == _languageIdByFileName.end()) {
    return nullptr;
  }
  return lookup_by_language_id_locked(_servicesByLanguageId, languageIt->second);
}

void DefaultServiceRegistry::removeLanguageMappingsLocked(
    std::string_view languageId, const CoreServices &services) {
  for (const auto &extension : services.languageMetaData.fileExtensions) {
    const auto normalized = normalize_extension(extension);
    if (normalized.empty()) {
      continue;
    }
    if (const auto it = _languageIdByExtension.find(normalized);
        it != _languageIdByExtension.end() && it->second == languageId) {
      _languageIdByExtension.erase(it);
    }
  }

  for (const auto &fileName : services.languageMetaData.fileNames) {
    if (fileName.empty()) {
      continue;
    }
    if (const auto it = _languageIdByFileName.find(fileName);
        it != _languageIdByFileName.end() && it->second == languageId) {
      _languageIdByFileName.erase(it);
    }
  }
}

void DefaultServiceRegistry::addLanguageMappingsLocked(
    std::string_view languageId, const CoreServices &services) {
  for (const auto &extension : services.languageMetaData.fileExtensions) {
    const auto normalized = normalize_extension(extension);
    if (normalized.empty()) {
      continue;
    }
    if (const auto it = _languageIdByExtension.find(normalized);
        it != _languageIdByExtension.end() && it->second != languageId) {
      publish_language_mapping_collision(
          *shared.observabilitySink, languageId,
          "The file extension " + normalized +
              " is used by multiple languages. It is now assigned to '" +
              std::string(languageId) + "'.");
    }
    _languageIdByExtension.insert_or_assign(normalized, std::string(languageId));
  }

  for (const auto &fileName : services.languageMetaData.fileNames) {
    if (fileName.empty()) {
      continue;
    }
    if (const auto it = _languageIdByFileName.find(fileName);
        it != _languageIdByFileName.end() && it->second != languageId) {
      publish_language_mapping_collision(
          *shared.observabilitySink, languageId,
          "The file name " + fileName +
              " is used by multiple languages. It is now assigned to '" +
              std::string(languageId) + "'.");
    }
    _languageIdByFileName.insert_or_assign(fileName, std::string(languageId));
  }
}

} // namespace pegium::services
