#include <@PEGIUM_NEW_LANGUAGE_ID@/lsp/Module.hpp>

#include <pegium/lsp/runtime/LanguageServerRuntime.hpp>

int main(int argc, char **argv) {
  return pegium::runLanguageServerMain(argc, argv, "@PEGIUM_NEW_LANGUAGE_ID@-lsp",
                                       @PEGIUM_NEW_LANGUAGE_ID@::lsp::register@PEGIUM_NEW_CLASS@Services);
}
