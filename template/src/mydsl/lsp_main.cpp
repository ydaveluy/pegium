#include <mydsl/lsp/Module.hpp>

#include <pegium/lsp/runtime/LanguageServerRuntime.hpp>

int main(int argc, char **argv) {
  return pegium::runLanguageServerMain(argc, argv, "mydsl-lsp",
                                       mydsl::lsp::registerMyDslServices);
}
