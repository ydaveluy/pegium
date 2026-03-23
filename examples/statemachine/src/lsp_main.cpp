#include <statemachine/services/Module.hpp>

#include <pegium/lsp/runtime/LanguageServerRuntime.hpp>

int main(int argc, char **argv) {
  return pegium::runLanguageServerMain(
      argc, argv, "statemachine-lsp",
      statemachine::services::register_language_services);
}
