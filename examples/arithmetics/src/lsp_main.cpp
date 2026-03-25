#include <arithmetics/lsp/Module.hpp>

#include <pegium/lsp/runtime/LanguageServerRuntime.hpp>

int main(int argc, char **argv) {
  return pegium::runLanguageServerMain(
      argc, argv, "arithmetics-lsp",
      arithmetics::lsp::register_language_services);
}
