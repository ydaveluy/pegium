#pragma once

#include <string>
#include <string_view>

namespace requirements {

std::string decode_quoted_string(std::string_view text);

} // namespace requirements
