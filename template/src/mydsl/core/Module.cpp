#include <mydsl/core/Module.hpp>

#include <utility>

#include <mydsl/parser/Parser.hpp>

namespace mydsl {

namespace {
template <typename Services>
void applyMyDslCoreModule(Services &services) {
  services.parser = std::make_unique<const parser::MyDslParser>(services);
  services.languageMetaData.fileExtensions = {".mydsl"};
}
} // namespace

void installMyDslCoreModule(MyDslCoreServices &services) {
  applyMyDslCoreModule(services);
}

std::unique_ptr<MyDslCoreServices>
createMyDslServices(const pegium::SharedCoreServices &sharedServices,
                    std::string languageId) {
  auto services = std::make_unique<MyDslCoreServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  installMyDslCoreModule(*services);
  return services;
}

bool registerMyDslServices(pegium::SharedCoreServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createMyDslServices(sharedServices));
  return true;
}

} // namespace mydsl
