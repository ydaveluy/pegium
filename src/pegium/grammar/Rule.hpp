#pragma once
#include <concepts>
#include <pegium/grammar/AbstractRule.hpp>
#include <pegium/grammar/DataTypeRule.hpp>
#include <pegium/grammar/ParserRule.hpp>
#include <pegium/grammar/TerminalRule.hpp>

namespace pegium::grammar {

template <typename T>
concept IsParserRule = std::derived_from<std::remove_cvref_t<T>, ParserRule>;
template <typename T>
concept IsDataTypeRule =
    std::derived_from<std::remove_cvref_t<T>, DataTypeRule>;
template <typename T>
concept IsTerminalRule =
    std::derived_from<std::remove_cvref_t<T>, TerminalRule>;
template <typename T>
concept IsRule = std::derived_from<std::remove_cvref_t<T>, AbstractRule>;

} // namespace pegium::grammar
