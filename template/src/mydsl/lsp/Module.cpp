#include <mydsl/lsp/Module.hpp>

#include <utility>

#include <mydsl/parser/Parser.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace mydsl {

namespace {
template <typename Services>
void applyMyDslCoreModule(Services &services) {
  services.parser = std::make_unique<const parser::MyDslParser>(services);
  services.languageMetaData.fileExtensions = {".mydsl"};
}
} // namespace

void installMyDslCoreModule(lsp::MyDslServices &services) {
  applyMyDslCoreModule(services);
}

} // namespace mydsl

namespace mydsl::lsp {

void installMyDslLspModule(MyDslServices &services) {
  // Wire LSP feature overrides here, e.g. a custom formatter:
  //   services.lsp.formatter = std::make_unique<MyDslFormatter>(services);
  (void)services;
}

std::unique_ptr<MyDslServices>
createMyDslServices(const pegium::SharedServices &sharedServices,
                    std::string languageId) {
  auto services = pegium::makeDefaultServices<MyDslServices>(
      sharedServices, std::move(languageId));
  mydsl::installMyDslCoreModule(*services);
  installMyDslLspModule(*services);
  return services;
}

bool registerMyDslServices(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createMyDslServices(sharedServices));
  return true;
}

} // namespace mydsl::lsp
