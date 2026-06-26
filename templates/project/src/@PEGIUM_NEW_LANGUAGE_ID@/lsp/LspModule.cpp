#include <@PEGIUM_NEW_LANGUAGE_ID@/lsp/LspModule.hpp>

#include <utility>

#include <@PEGIUM_NEW_LANGUAGE_ID@/core/CoreModule.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace @PEGIUM_NEW_LANGUAGE_ID@ {

std::unique_ptr<@PEGIUM_NEW_CLASS@Services>
create@PEGIUM_NEW_CLASS@LspServices(const pegium::SharedServices &sharedServices,
                     std::string languageId) {
  auto services = pegium::makeDefaultServices<@PEGIUM_NEW_CLASS@Services>(
      sharedServices, std::move(languageId));
  // @PEGIUM_NEW_CLASS@Services is-a pegium::CoreServices, so it binds the
  // (non-template) core wiring directly.
  install@PEGIUM_NEW_CLASS@CoreModule(*services);
  return services;
}

bool register@PEGIUM_NEW_CLASS@LspServices(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      create@PEGIUM_NEW_CLASS@LspServices(sharedServices));
  return true;
}

} // namespace @PEGIUM_NEW_LANGUAGE_ID@
