#pragma once

#include <pegium/grammar/Action.hpp>
#include <pegium/parser/AbstractElement.hpp>
#include <type_traits>

namespace pegium::parser {

template <typename T, auto feature> struct Action final : grammar::Action {

  // explicit Action() {}

  constexpr ElementKind getKind() const noexcept final {
    if constexpr (feature)
      return ElementKind::Init;
    else
      return ElementKind::New;
  }

  std::shared_ptr<AstNode>
  execute(std::shared_ptr<AstNode> current) const override {
    if constexpr (feature)
      return do_execute(current, feature);
    else
      return std::make_shared<T>();
  }
  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const {

    auto &node = parent.content.emplace_back();
    node.grammarSource = this;
    node.text = {sv.data(), 0};
    return MatchResult::success(sv.begin());
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return MatchResult::success(sv.begin());
  }
  void print(std::ostream &os) const override {
    if constexpr (feature)
      os << "new " << typeid(T).name() << "(current)"; // TODO add feature name
    else
      os << "new " << typeid(T).name() << "()";
  }

private:
  template <typename ClassType, typename AttrType>
  std::shared_ptr<AstNode> do_execute(std::shared_ptr<AstNode> current,
                                      AttrType ClassType::*member) const {
    auto result = std::make_shared<T>();

    auto value = std::dynamic_pointer_cast<helpers::AttrType<feature>>(current);
    helpers::AssignmentHelper<AttrType>{}(result.get()->*member,
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