#pragma once

#include <memory>

#include <@PEGIUM_NEW_LANGUAGE_ID@/core/Services.hpp>

namespace @PEGIUM_NEW_LANGUAGE_ID@::parser {

// The parser is declared here but defined in ModuleImpl.cpp, so the heavy
// grammar template instantiations happen in that single translation unit instead
// of in every TU that wires the module (the core and lsp modules).
std::unique_ptr<const pegium::parser::Parser>
make@PEGIUM_NEW_CLASS@Parser(const pegium::CoreServices &services);

} // namespace @PEGIUM_NEW_LANGUAGE_ID@::parser

namespace @PEGIUM_NEW_LANGUAGE_ID@::detail {

/// Core service overrides shared by the core and lsp modules.
template <typename Services>
void apply@PEGIUM_NEW_CLASS@CoreModule(Services &services) {
  services.parser = parser::make@PEGIUM_NEW_CLASS@Parser(services);
  services.languageMetaData.fileExtensions = {"@PEGIUM_NEW_EXT@"};
}

} // namespace @PEGIUM_NEW_LANGUAGE_ID@::detail
