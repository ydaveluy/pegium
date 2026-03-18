#include <pegium/parser/PegiumParser.hpp>

#include <ostream>
#include <string_view>

namespace pegium::parser {
namespace {

[[nodiscard]] constexpr std::string_view
parse_diagnostic_kind_name(ParseDiagnosticKind kind) noexcept {
  using enum ParseDiagnosticKind;
  switch (kind) {
  case Inserted:
    return "Inserted";
  case Deleted:
    return "Deleted";
  case Replaced:
    return "Replaced";
  case Incomplete:
    return "Incomplete";
  case Recovered:
    return "Recovered";
  case ConversionError:
    return "ConversionError";
  }
  return "Unknown";
}

} // namespace

std::ostream &operator<<(std::ostream &os, ParseDiagnosticKind kind) {
  return os << parse_diagnostic_kind_name(kind);
}

} // namespace pegium::parser
