#include <@PEGIUM_NEW_LANGUAGE_ID@/lsp/LspModule.hpp>

#include <pegium/lsp/runtime/LanguageServerRuntime.hpp>

int main(int argc, char **argv) {
  return pegium::runLanguageServerMain(argc, argv, "@PEGIUM_NEW_LANGUAGE_ID@-lsp",
                                       @PEGIUM_NEW_LANGUAGE_ID@::register@PEGIUM_NEW_CLASS@LspServices);
}
