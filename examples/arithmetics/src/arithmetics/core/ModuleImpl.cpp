#include <arithmetics/core/ModuleImpl.hpp>
#include <arithmetics/core/ArithmeticParser.hpp>

namespace arithmetics::parser {

std::unique_ptr<const pegium::parser::Parser>
makeArithmeticParser(const pegium::CoreServices &services) {
  return std::make_unique<const ArithmeticParser>(services);
}

} // namespace arithmetics::parser
