#pragma once

#include <concepts>
#include <memory>
#include <pegium/core/grammar/Create.hpp>
#include <pegium/core/parser/Introspection.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/ParseExpression.hpp>
#include <pegium/core/parser/AstReflectionBootstrap.hpp>
#include <string_view>

namespace pegium::parser {

template <typename T>
  requires DefaultConstructibleAstNode<T>
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
  friend struct detail::InitAccess;

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    if constexpr (ExpectParseModeContext<Context>) {
      (void)ctx;
      return true;
    } else {
      ctx.leaf(ctx.cursor(), this);
      return true;
    }
  }

  void init_impl(AstReflectionInitContext &ctx) const {
    ctx.registerProducedType(detail::ast_node_type_info<T>());
  }
};

/// Create a new parser-managed AST shell of type `T`.
///
/// `T` must be a default-constructible concrete `AstNode` because value
/// building assigns the node fields after creation instead of calling a
/// semantic constructor with all arguments.
template <typename T>
  requires DefaultConstructibleAstNode<T>
static constexpr auto create() {
  return Create<T>();
}

} // namespace pegium::parser
