#include <pegium/grammar/Literal.hpp>

#include <cctype>
#include <format>
#include <ostream>
#include <string_view>

#include <pegium/syntax-tree/CstNodeView.hpp>

namespace pegium::grammar {

namespace {

void printEscapedChar(std::ostream &os, char c) {
  switch (c) {
  case '\n':
    os << R"(\n)";
    return;
  case '\r':
    os << R"(\r)";
    return;
  case '\t':
    os << R"(\t)";
    return;
  case '\v':
    os << R"(\v)";
    return;
  case '\f':
    os << R"(\f)";
    return;
  case '\b':
    os << R"(\b)";
    return;
  case '\a':
    os << R"(\a)";
    return;
  case '\\':
    os << R"(\\)";
    return;
  case '\'':
    os << R"(\')";
    return;
  case '\"':
    os << R"(\")";
    return;
  default:
    break;
  }

  if (std::isprint(static_cast<unsigned char>(c)) != 0) {
    os << c;
    return;
  }

  os << std::format("\\x{:02X}", static_cast<unsigned char>(c));
}

} // namespace

std::string_view Literal::getValue(const CstNodeView &node) const noexcept {
  return node.getText();
}

void Literal::print(std::ostream &os) const {
  os << '\'';
  for (const char c : getValue()) {
    printEscapedChar(os, c);
  }
  os << '\'';
  if (!isCaseSensitive()) {
    os << 'i';
  }
}

} // namespace pegium::grammar
