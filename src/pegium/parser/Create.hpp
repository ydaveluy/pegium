#pragma once

#include <concepts>
#include <memory>
#include <pegium/grammar/Create.hpp>
#include <pegium/parser/Introspection.hpp>
#include <pegium/parser/ParseMode.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <string_view>

namespace pegium::parser {

template <typename T>
struct Create final : grammar::Create {
  static constexpr bool nullable = true;
  static constexpr bool isFailureSafe = true;

  std::unique_ptr<AstNode> getValue() const override {
    return std::make_unique<T>();
  }

  std::string_view getTypeName() const noexcept override {
    static constexpr auto typeName = detail::type_name_v<T>;
    return typeName;
  }

  constexpr bool isNullable() const noexcept override {
    return nullable;
  }

private:
  friend struct detail::ParseAccess;

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    if constexpr (ExpectParseModeContext<Context>) {
      (void)ctx;
      return true;
    } else {
      ctx.leaf(ctx.cursor(), this);
      return true;
    }
  }
};

/// Create a new instance of type T.
template <typename T>
  requires std::derived_from<T, AstNode>
static constexpr auto create() {
  return Create<T>();
}

} // namespace pegium::parser
