#include <@PEGIUM_NEW_LANGUAGE_ID@/core/Module.hpp>

#include <utility>

#include <@PEGIUM_NEW_LANGUAGE_ID@/core/Parser.hpp>

namespace @PEGIUM_NEW_LANGUAGE_ID@ {

namespace {
template <typename Services> void apply@PEGIUM_NEW_CLASS@CoreModule(Services &services) {
  services.parser = std::make_unique<const parser::@PEGIUM_NEW_CLASS@Parser>(services);
  services.languageMetaData.fileExtensions = {"@PEGIUM_NEW_EXT@"};
}
} // namespace

std::unique_ptr<@PEGIUM_NEW_CLASS@CoreServices>
create@PEGIUM_NEW_CLASS@Services(const pegium::SharedCoreServices &sharedServices,
                     std::string languageId) {
  auto services = std::make_unique<@PEGIUM_NEW_CLASS@CoreServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  apply@PEGIUM_NEW_CLASS@CoreModule(*services);
  return services;
}

bool register@PEGIUM_NEW_CLASS@Services(pegium::SharedCoreServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      create@PEGIUM_NEW_CLASS@Services(sharedServices));
  return true;
}

} // namespace @PEGIUM_NEW_LANGUAGE_ID@
