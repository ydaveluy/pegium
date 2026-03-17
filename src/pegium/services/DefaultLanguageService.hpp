#pragma once

#include <pegium/services/Services.hpp>

namespace pegium::services {

class DefaultLanguageService {
public:
  explicit DefaultLanguageService(const Services &languageServices)
      : languageServices(languageServices) {}
  virtual ~DefaultLanguageService() noexcept = default;

protected:
  const Services &languageServices;
};

} // namespace pegium::services
