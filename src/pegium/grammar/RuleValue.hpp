#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace pegium::grammar {

using RuleValue = std::variant<
    std::int8_t, std::int16_t, std::int32_t, std::int64_t, std::uint8_t,
    std::uint16_t, std::uint32_t, std::uint64_t, float, double, long double,
    char, std::string_view, bool, std::string, std::nullptr_t>;

} // namespace pegium::grammar
