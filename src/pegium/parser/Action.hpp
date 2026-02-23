#pragma once

#include <pegium/grammar/Action.hpp>
#include <pegium/parser/AssignmentHelpers.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/Introspection.hpp>
#include <type_traits>

namespace pegium::parser {

template <typename T, auto feature> struct Action final : grammar::Action {
  static constexpr bool nullable = true; 

  constexpr ElementKind getKind() const noexcept override {
    if constexpr (feature != nullptr)
      return ElementKind::Init;
    else
      return ElementKind::New;
  }

  std::shared_ptr<AstNode>
  execute(std::shared_ptr<AstNode> current) const override {
    if constexpr (feature != nullptr)
      return do_execute(std::move(current), feature);
    else
      return std::make_shared<T>();
  }
  bool rule(ParseContext &ctx) const {
    ctx.leaf(ctx.cursor(), this);
    return true;
  }
  constexpr MatchResult terminal(const char *begin,
                                       const char *) const noexcept {
    return MatchResult::success(begin);
  }
  constexpr MatchResult terminal(std::string_view sv) const noexcept {
    return terminal(sv.begin(), sv.end());
  }


  std::string_view getTypeName() const noexcept override {
    static constexpr auto typeName = detail::type_name_v<T>;
    return typeName;
  }
  void print(std::ostream &os) const override {
    if constexpr (feature != nullptr)
      os << "new " << getTypeName() << "(current)"; // TODO add feature name
    else
      os << "new " << getTypeName() << "()";
  }

private:
  template <typename ClassType, typename AttrType>
  std::shared_ptr<AstNode> do_execute(std::shared_ptr<AstNode> current,
                                      AttrType ClassType::*member) const {
    auto result = std::make_shared<T>();
    assert(std::dynamic_pointer_cast<helpers::AttrType<feature>>(current));
    auto value = std::static_pointer_cast<helpers::AttrType<feature>>(current);
    helpers::AssignmentHelper<AttrType>{}(result.get(), member,
                                          std::move(value));
    return result;
  }
};

/// Create a new instance of type T and assign the given feature with the
/// current value
/// @tparam T the new instance class
/// @tparam feature the feature to assign
template <typename T, auto feature>
  requires std::is_member_pointer_v<
      decltype(feature)> //&&            std::derived_from<T,
                         // helpers::AttrType<feature>>
static constexpr auto action() {
  return Action<T, feature>();
}

/// Create a new instance and assign the given feature with the current value
/// The instance type is deduced from the feature
/// @tparam feature the feature to assign
template <auto feature>
  requires std::is_member_pointer_v<decltype(feature)>
static constexpr auto action() {
  return Action<helpers::ClassType<feature>, feature>();
}
/// Create a new instance of type T
/// @tparam T the new instance class
template <typename T>
  requires std::derived_from<T, AstNode>
static constexpr auto action() {
  return Action<T, nullptr>();
}

} // namespace pegium::parser
