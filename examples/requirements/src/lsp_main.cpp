#include <requirements/services/Module.hpp>

#include <pegium/lsp/runtime/LanguageServerRuntime.hpp>

int main(int argc, char **argv) {
  return pegium::runLanguageServerMain(
      argc, argv, "requirements-lsp",
      requirements::services::register_language_services);
}
