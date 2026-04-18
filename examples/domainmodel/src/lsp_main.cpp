#include <domainmodel/lsp/Module.hpp>

#include <pegium/lsp/runtime/LanguageServerRuntime.hpp>

int main(int argc, char **argv) {
  return pegium::runLanguageServerMain(
      argc, argv, "domainmodel-lsp",
      domainmodel::lsp::registerDomainModelServices);
}
