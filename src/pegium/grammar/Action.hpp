#pragma once

#include <pegium/grammar/IAction.hpp>
#include <type_traits>

namespace pegium::grammar {
template <typename T, auto feature> struct Action final: IAction {

  explicit Action() {}

  std::shared_ptr<AstNode>
  execute(std::shared_ptr<AstNode> current) const override {

    if constexpr (std::is_same_v<std::nullptr_t, decltype(feature)>) {
      return std::make_shared<T>();
    } else {
      return do_execute(current, feature);
    }
  }
  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const override {

    auto &node = parent.content.emplace_back();
    node.grammarSource = this;
    node.text = {sv.data(), 0};
    return MatchResult::success(sv.begin());
  }
  constexpr MatchResult
  parse_terminal(std::string_view sv) const noexcept override {
    return MatchResult::success(sv.begin());
  }
  void print(std::ostream &os) const override {
    os << "Action(" << typeid(T).name() << ")";
  }

  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::Action;
  }

private:
  template <typename ClassType, typename AttrType>
  std::shared_ptr<AstNode> do_execute(std::shared_ptr<AstNode> current,
                                      AttrType ClassType::*member) const {
    auto result = std::make_shared<T>();
    
    auto value = std::dynamic_pointer_cast<helpers::AttrType<feature>>(current);
    helpers::AssignmentHelper<AttrType>{}(result.get()->*member , std::move(value));
    return result;
  }
};
template <typename T, auto feature>
  requires std::is_member_pointer_v<decltype(feature)> //&&            std::derived_from<T, helpers::AttrType<feature>>
static constexpr auto action() {
  return Action<T, feature>();
}

template <auto feature>
  requires std::is_member_pointer_v<decltype(feature)>
static constexpr auto action() {
  return Action<helpers::ClassType<feature>, feature>();
}
template <typename T> static constexpr auto action() {
  return Action<T, nullptr>();
}

} // namespace pegium::grammar