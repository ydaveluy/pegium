#include <pegium/core/utils/TextUtils.hpp>

#include <cctype>
#include <string>

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
    static constexpr char kHexDigits[] = "0123456789ABCDEF";
    const auto byte = static_cast<unsigned char>(c);
    return std::string{'\\', 'x', kHexDigits[byte >> 4],
                       kHexDigits[byte & 0x0F]};
  }
}

} // namespace pegium::utils
