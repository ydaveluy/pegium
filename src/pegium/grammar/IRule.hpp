#pragma once
#include <concepts>
#include <pegium/IParser.hpp>
#include <pegium/grammar/IGrammarElement.hpp>
#include <pegium/grammar/IContext.hpp>
#include <string_view>
#include <any>

namespace pegium::grammar {

struct IRule : IGrammarElement{
  virtual pegium::GenericParseResult parseGeneric(std::string_view text, std::unique_ptr<pegium::grammar::IContext> context) const = 0;
  virtual std::any getAnyValue(const CstNode &node) const = 0;
};

template <typename T>
concept IsRule = std::derived_from<std::remove_cvref_t<T>, IRule>;
} // namespace pegium::grammar