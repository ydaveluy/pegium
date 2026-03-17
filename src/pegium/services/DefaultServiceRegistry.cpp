#include <pegium/services/DefaultServiceRegistry.hpp>

#include <algorithm>
#include <filesystem>

#include <pegium/services/CoreServices.hpp>
#include <pegium/utils/UriUtils.hpp>

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
    const std::unordered_map<std::string, std::unique_ptr<CoreServices>>
        &servicesByLanguageId,
    std::string_view languageId) {
  const auto it = servicesByLanguageId.find(std::string(languageId));
  return it == servicesByLanguageId.end() ? nullptr : it->second.get();
}

} // namespace

void DefaultServiceRegistry::setTextDocuments(
    const workspace::TextDocuments *textDocuments) noexcept {
  std::scoped_lock lock(_mutex);
  _textDocuments = textDocuments;
}

bool DefaultServiceRegistry::registerServices(
    std::unique_ptr<CoreServices> services) {
  if (!services || services->languageId.empty() || !services->isComplete()) {
    return false;
  }

  const std::string languageId = services->languageId;
  if (services->languageMetaData.languageId.empty()) {
    services->languageMetaData.languageId = languageId;
  }
  std::scoped_lock lock(_mutex);
  auto [it, inserted] = _servicesByLanguageId.try_emplace(languageId);
  if (inserted) {
    _registrationOrder.push_back(languageId);
  }
  it->second = std::move(services);
  rebuildUriMapsLocked();
  return true;
}

const CoreServices *
DefaultServiceRegistry::getServicesByLanguageId(
    std::string_view languageId) const {
  std::scoped_lock lock(_mutex);
  return lookup_by_language_id_locked(_servicesByLanguageId, languageId);
}

const CoreServices *
DefaultServiceRegistry::getServices(std::string_view uri) const {
  std::scoped_lock lock(_mutex);
  if (_servicesByLanguageId.empty()) {
    return nullptr;
  }

  const auto normalizedUri = utils::normalize_uri(uri);
  if (_textDocuments != nullptr) {
    if (auto textDocument = _textDocuments->get(normalizedUri);
        textDocument != nullptr && !textDocument->languageId.empty()) {
      if (const auto *services = lookup_by_language_id_locked(
              _servicesByLanguageId, textDocument->languageId)) {
        return services;
      }
    }
  }

  if (const auto path = utils::file_uri_to_path(normalizedUri); path.has_value()) {
    const auto filePath = std::filesystem::path(*path);
    if (const auto fileName = filePath.filename().string();
        !fileName.empty()) {
      if (const auto *services = lookupByFileName(fileName)) {
        return services;
      }
    }
    if (const auto extension = filePath.extension().string();
        !extension.empty()) {
      if (const auto *services = lookupByExtension(extension)) {
        return services;
      }
    }
  } else {
    if (const auto *services = lookupByFileName(normalizedUri)) {
      return services;
    }
    const auto extension =
        std::filesystem::path(std::string(normalizedUri)).extension().string();
    if (!extension.empty()) {
      if (const auto *services = lookupByExtension(extension)) {
        return services;
      }
    }
  }

  return nullptr;
}

bool DefaultServiceRegistry::hasServices(std::string_view uri) const {
  return getServices(uri) != nullptr;
}

std::vector<const CoreServices *> DefaultServiceRegistry::all() const {
  std::scoped_lock lock(_mutex);
  std::vector<const CoreServices *> services;
  services.reserve(_registrationOrder.size());
  for (const auto &languageId : _registrationOrder) {
    const auto *service =
        lookup_by_language_id_locked(_servicesByLanguageId, languageId);
    if (service != nullptr) {
      services.push_back(service);
    }
  }
  return services;
}

const CoreServices *
DefaultServiceRegistry::getServicesByFileName(std::string_view fileName) const {
  std::scoped_lock lock(_mutex);
  if (_servicesByLanguageId.empty()) {
    return nullptr;
  }

  if (const auto *services = lookupByFileName(fileName)) {
    return services;
  }

  const auto extension =
      std::filesystem::path(std::string(fileName)).extension().string();
  if (!extension.empty()) {
    if (const auto *services = lookupByExtension(extension)) {
      return services;
    }
  }

  return nullptr;
}

bool DefaultServiceRegistry::remove(std::string_view languageId) {
  std::scoped_lock lock(_mutex);
  if (_servicesByLanguageId.erase(std::string(languageId)) == 0) {
    return false;
  }
  _registrationOrder.erase(
      std::remove(_registrationOrder.begin(), _registrationOrder.end(),
                  std::string(languageId)),
      _registrationOrder.end());
  rebuildUriMapsLocked();
  return true;
}

const CoreServices *
DefaultServiceRegistry::lookupByExtension(std::string_view extension) const {
  const auto languageIt =
      _languageIdByExtension.find(normalize_extension(extension));
  if (languageIt == _languageIdByExtension.end()) {
    return nullptr;
  }
  const auto servicesIt = _servicesByLanguageId.find(languageIt->second);
  return servicesIt == _servicesByLanguageId.end() ? nullptr
                                                   : servicesIt->second.get();
}

const CoreServices *
DefaultServiceRegistry::lookupByFileName(std::string_view fileName) const {
  const auto languageIt = _languageIdByFileName.find(std::string(fileName));
  if (languageIt == _languageIdByFileName.end()) {
    return nullptr;
  }
  const auto servicesIt = _servicesByLanguageId.find(languageIt->second);
  return servicesIt == _servicesByLanguageId.end() ? nullptr
                                                   : servicesIt->second.get();
}

void DefaultServiceRegistry::rebuildUriMapsLocked() {
  _languageIdByExtension.clear();
  _languageIdByFileName.clear();

  for (const auto &languageId : _registrationOrder) {
    const auto serviceIt = _servicesByLanguageId.find(languageId);
    if (serviceIt == _servicesByLanguageId.end()) {
      continue;
    }
    const auto &services = serviceIt->second;
    if (services == nullptr) {
      continue;
    }
    for (const auto &extension : services->languageMetaData.fileExtensions) {
      const auto normalized = normalize_extension(extension);
      if (!normalized.empty()) {
        _languageIdByExtension.insert_or_assign(normalized, languageId);
      }
    }
    for (const auto &fileName : services->languageMetaData.fileNames) {
      if (!fileName.empty()) {
        _languageIdByFileName.insert_or_assign(fileName, languageId);
      }
    }
  }
}

} // namespace pegium::services
