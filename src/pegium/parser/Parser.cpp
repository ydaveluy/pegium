#include <pegium/parser/PegiumParser.hpp>

#include <ostream>
#include <string_view>

namespace pegium::parser {
namespace {

[[nodiscard]] constexpr std::string_view
parse_diagnostic_kind_name(ParseDiagnosticKind kind) noexcept {
  switch (kind) {
  case ParseDiagnosticKind::Inserted:
    return "Inserted";
  case ParseDiagnosticKind::Deleted:
    return "Deleted";
  case ParseDiagnosticKind::Replaced:
    return "Replaced";
  case ParseDiagnosticKind::Incomplete:
    return "Incomplete";
  case ParseDiagnosticKind::Recovered:
    return "Recovered";
  case ParseDiagnosticKind::ConversionError:
    return "ConversionError";
  }
  return "Unknown";
}

} // namespace

std::ostream &operator<<(std::ostream &os, ParseDiagnosticKind kind) {
  return os << parse_diagnostic_kind_name(kind);
}

std::ostream &operator<<(std::ostream &os,
                         const ParseDiagnostic &diagnostic) {
  os << "ParseDiagnostic{kind=" << diagnostic.kind
     << ", offset=" << diagnostic.offset
     << ", begin=" << diagnostic.beginOffset
     << ", end=" << diagnostic.endOffset;
  if (diagnostic.element != nullptr) {
    os << ", element=" << *diagnostic.element;
  }
  if (!diagnostic.message.empty()) {
    os << ", message=\"" << diagnostic.message << "\"";
  }
  os << "}";
  return os;
}

} // namespace pegium::parser
