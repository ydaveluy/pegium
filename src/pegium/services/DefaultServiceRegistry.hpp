#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <pegium/services/ServiceRegistry.hpp>
#include <pegium/utils/TransparentStringHash.hpp>
#include <pegium/workspace/TextDocuments.hpp>

namespace pegium::services {

class DefaultServiceRegistry : public ServiceRegistry {
public:
  void
  setTextDocuments(const workspace::TextDocuments *textDocuments) noexcept;

  bool registerServices(std::unique_ptr<CoreServices> services) override;

  [[nodiscard]] const CoreServices *
  getServices(std::string_view uri) const override;
  [[nodiscard]] bool hasServices(std::string_view uri) const override;
  [[nodiscard]] std::vector<const CoreServices *> all() const override;

  [[nodiscard]] const CoreServices *
  getServicesByLanguageId(std::string_view languageId) const override;
  [[nodiscard]] const CoreServices *
  getServicesByFileName(std::string_view fileName) const override;

  bool remove(std::string_view languageId) override;

private:
  [[nodiscard]] const CoreServices *
  lookupByExtension(std::string_view extension) const;
  [[nodiscard]] const CoreServices *
  lookupByFileName(std::string_view fileName) const;
  void rebuildUriMapsLocked();

  mutable std::mutex _mutex;
  utils::TransparentStringMap<std::unique_ptr<CoreServices>> _servicesByLanguageId;
  std::vector<std::string> _registrationOrder;
  utils::TransparentStringMap<std::string> _languageIdByExtension;
  utils::TransparentStringMap<std::string> _languageIdByFileName;
  const workspace::TextDocuments *_textDocuments = nullptr;
};

} // namespace pegium::services
