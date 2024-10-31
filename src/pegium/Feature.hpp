#pragma once

#include <any>
#include <concepts>
#include <pegium/syntax-tree.hpp>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace pegium {

// Returns the current function's name as a string view
template <auto e> static constexpr std::string_view function_name() noexcept {
  return std::source_location::current().function_name();
}

/// A generic holder for a feature
struct FeatureHolder {

  template <auto e>
  static FeatureHolder from()
    requires std::is_member_object_pointer_v<decltype(e)>
  {
    FeatureHolder f;
    f.member = e;
    f.name = member_name<e>();
    f.equal = [](const FeatureHolder &lhs, const FeatureHolder &rhs) -> bool {
      return rhs.member.type() == typeid(e) &&
             std::any_cast<decltype(e)>(lhs.member) ==
                 std::any_cast<decltype(e)>(rhs.member);
    };
    f.set = setter(e);
    return f;
  }

  inline const std::string &getName() const noexcept { return name; }
  bool operator==(const FeatureHolder &other) const noexcept {

    return equal(*this, other);
  }

private:
  // create a setter for a reference
  template <typename R, typename C>
    requires std::derived_from<C, AstNode>
  static auto setter(Reference<R> C::*feature) {
    return [feature](AstNode *object, const std::any &value) {
      auto *obj = dynamic_cast<C *>(object);
      assert(obj);
      // store the reference as string
      obj->*feature = std::any_cast<std::string>(value);
      // TODO initialize resolve from the Context
    };
  }

  // create a setter for an attribute of data type
  template <typename R, typename C>
    requires std::derived_from<C, AstNode> && is_data_type_v<R>
 static auto setter(R C::*feature) {
    return [feature](AstNode *object, const std::any &value) {
      auto *obj = dynamic_cast<C *>(object);
      assert(obj);
      obj->*feature = std::any_cast<R>(value);
    };
  }
  // create a setter for an attribute of AstNode
  template <typename R, typename C>
    requires std::derived_from<C, AstNode> && std::derived_from<R, AstNode>
 static auto setter(std::shared_ptr<R> C::*feature) {
    return [feature](AstNode *object, const std::any &value) {
      auto *obj = dynamic_cast<C *>(object);
      assert(obj);
      obj->*feature = std::dynamic_pointer_cast<R>(
          std::any_cast<std::shared_ptr<AstNode>>(value));
    };
  }

  // create a setter for a vector of reference
  template <typename R, typename C>
    requires std::derived_from<C, AstNode> && is_data_type_v<R>
 static auto setter(std::vector<Reference<R>> C::*feature) {
    return [](AstNode *object, const std::any &fea, const std::any &value) {
      auto *obj = dynamic_cast<C *>(object);
      assert(obj);
      auto &member = obj->*std::any_cast<std::vector<Reference<R>> C::*>(fea);
      member.emplace_back() = std::any_cast<std::string>(value);
    };
  }
  // create a setter for a vector of data type
  template <typename R, typename C>
    requires std::derived_from<C, AstNode> && is_data_type_v<R>
 static auto setter(std::vector<R> C::*feature) {
    return [](AstNode *object, const std::any &fea, const std::any &value) {
      auto *obj = dynamic_cast<C *>(object);
      assert(obj);
      auto &member = obj->*std::any_cast<std::vector<R> C::*>(fea);
      member.emplace_back(std::any_cast<R>(value));
    };
  }

  // create a setter for a vector of AstNode
  template <typename R, typename C>
    requires std::derived_from<C, AstNode> && std::derived_from<R, AstNode>
 static auto setter(std::vector<std::shared_ptr<R>> C::*feature) {
    return [](AstNode *object, const std::any &fea, const std::any &value) {
      auto *obj = dynamic_cast<C *>(object);
      assert(obj);
      auto &member =
          obj->*std::any_cast<std::vector<std::shared_ptr<R>> C::*>(fea);
      member.emplace_back(std::dynamic_pointer_cast<R>(
          std::any_cast<std::shared_ptr<AstNode>>(value)));
    };
  }

  FeatureHolder() = default;
  std::any member;
  std::string name;
  std::function<bool(const FeatureHolder &, const FeatureHolder &)> equal;
  std::function<void(AstNode *, const std::any &)> set;

  /// Helper to get the name of a member from a member object pointer
  /// @tparam e the member object pointer
  /// @return the name of the member
  template <auto e> static constexpr std::string_view member_name() noexcept {
    std::string_view func_name = function_name<e>();
    func_name = func_name.substr(0, func_name.rfind(REF_STRUCT::end_marker));
    return func_name.substr(func_name.rfind("::") + 2);
  }
  struct REF_STRUCT {
    int MEMBER;

    static constexpr auto name = function_name<&REF_STRUCT::MEMBER>();
    static constexpr auto end_marker =
        name.substr(name.find("REF_STRUCT::MEMBER") +
                    std::string_view{"REF_STRUCT::MEMBER"}.size());
  };
};

} // namespace pegium
