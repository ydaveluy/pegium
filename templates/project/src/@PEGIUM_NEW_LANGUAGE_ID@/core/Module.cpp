#include <@PEGIUM_NEW_LANGUAGE_ID@/core/Module.hpp>

#include <utility>

#include <@PEGIUM_NEW_LANGUAGE_ID@/core/ModuleImpl.hpp>

namespace @PEGIUM_NEW_LANGUAGE_ID@ {

std::unique_ptr<@PEGIUM_NEW_CLASS@CoreServices>
create@PEGIUM_NEW_CLASS@Services(const pegium::SharedCoreServices &sharedServices,
                     std::string languageId) {
  auto services = std::make_unique<@PEGIUM_NEW_CLASS@CoreServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  detail::apply@PEGIUM_NEW_CLASS@CoreModule(*services);
  return services;
}

bool register@PEGIUM_NEW_CLASS@Services(pegium::SharedCoreServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      create@PEGIUM_NEW_CLASS@Services(sharedServices));
  return true;
}

} // namespace @PEGIUM_NEW_LANGUAGE_ID@
