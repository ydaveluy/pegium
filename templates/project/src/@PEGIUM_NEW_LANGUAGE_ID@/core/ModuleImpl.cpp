#include <@PEGIUM_NEW_LANGUAGE_ID@/core/ModuleImpl.hpp>

#include <@PEGIUM_NEW_LANGUAGE_ID@/core/@PEGIUM_NEW_CLASS@Parser.hpp>

namespace @PEGIUM_NEW_LANGUAGE_ID@::parser {

std::unique_ptr<const pegium::parser::Parser>
make@PEGIUM_NEW_CLASS@Parser(const pegium::CoreServices &services) {
  return std::make_unique<const @PEGIUM_NEW_CLASS@Parser>(services);
}

} // namespace @PEGIUM_NEW_LANGUAGE_ID@::parser
