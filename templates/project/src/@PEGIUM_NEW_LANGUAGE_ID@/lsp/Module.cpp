#include <@PEGIUM_NEW_LANGUAGE_ID@/lsp/Module.hpp>

#include <utility>

#include <@PEGIUM_NEW_LANGUAGE_ID@/core/@PEGIUM_NEW_CLASS@Parser.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace @PEGIUM_NEW_LANGUAGE_ID@::lsp {

std::unique_ptr<@PEGIUM_NEW_CLASS@Services>
create@PEGIUM_NEW_CLASS@Services(const pegium::SharedServices &sharedServices,
                     std::string languageId) {
  auto services = pegium::makeDefaultServices<@PEGIUM_NEW_CLASS@Services>(
      sharedServices, std::move(languageId));
  services->parser = std::make_unique<const parser::@PEGIUM_NEW_CLASS@Parser>(*services);
  services->languageMetaData.fileExtensions = {"@PEGIUM_NEW_EXT@"};
  return services;
}

bool register@PEGIUM_NEW_CLASS@Services(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      create@PEGIUM_NEW_CLASS@Services(sharedServices));
  return true;
}

} // namespace @PEGIUM_NEW_LANGUAGE_ID@::lsp
