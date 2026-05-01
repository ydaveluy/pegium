#include <pegium/core/utils/TextUtils.hpp>

#include <cctype>
#include <format>

namespace pegium::utils {

std::string escape_char(char c) {
  switch (c) {
  case '\n':
    return R"(\n)";
  case '\r':
    return R"(\r)";
  case '\t':
    return R"(\t)";
  case '\v':
    return R"(\v)";
  case '\f':
    return R"(\f)";
  case '\b':
    return R"(\b)";
  case '\a':
    return R"(\a)";
  case '\\':
    return R"(\\)";
  case '\'':
    return R"(\')";
  case '\"':
    return R"(\")";
  default:
    if (std::isprint(static_cast<unsigned char>(c))) {
      return std::string{c};
    }
    return std::format("\\x{:02X}", static_cast<unsigned char>(c));
  }
}

} // namespace pegium::utils
