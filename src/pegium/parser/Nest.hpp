#pragma once

#include <cassert>
#include <memory>
#include <pegium/grammar/Nest.hpp>
#include <pegium/parser/AssignmentHelpers.hpp>
#include <pegium/parser/Introspection.hpp>
#include <pegium/parser/ParseMode.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <string_view>
#include <type_traits>

namespace pegium::parser {

template <typename T, auto feature>
  requires std::is_member_pointer_v<decltype(feature)>
struct Nest final : grammar::Nest {
  static constexpr bool nullable = true;
  static constexpr bool isFailureSafe = true;

  std::unique_ptr<AstNode>
  getValue(std::unique_ptr<AstNode> current) const override {
    auto newNode = std::make_unique<T>();
    const ValueBuildContext context{.property = detail::member_name_v<feature>};
    using FeatureType = helpers::AttrType<feature>;
    assert(current != nullptr);
    assert(dynamic_cast<FeatureType *>(current.get()) != nullptr);
    auto *featureNode = static_cast<FeatureType *>(current.release());
    helpers::AssignmentHelper<helpers::MemberType<feature>>{}(
        newNode.get(), feature, std::unique_ptr<FeatureType>(featureNode),
        context);
    return newNode;
  }

  std::string_view getTypeName() const noexcept override {
    static constexpr auto typeName = detail::type_name_v<T>;
    return typeName;
  }

  std::string_view getFeature() const noexcept override {
    return detail::member_name_v<feature>;
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

/// Create a new instance of type T and assign the given feature with the
/// current value.
template <typename T, auto feature>
  requires std::is_member_pointer_v<decltype(feature)>
static constexpr auto nest() {
  return Nest<T, feature>();
}

/// Create a new instance and assign the given feature with the current value.
/// The instance type is deduced from the feature.
template <auto feature>
  requires std::is_member_pointer_v<decltype(feature)>
static constexpr auto nest() {
  return Nest<helpers::ClassType<feature>, feature>();
}

} // namespace pegium::parser
