#pragma once

#include <memory>
#include <pegium/syntax-tree/AstNode.hpp>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace pegium::parser::helpers {

template <auto Member> struct MemberTraits;

template <typename Class, typename Type, Type Class::*Member>
struct MemberTraits<Member> {
  using ClassType = Class;

  template <typename T> struct AttributeType {
    using type = T;
    static constexpr bool isMany = false;
  };
  template <typename T> struct AttributeType<std::shared_ptr<T>> {
    using type = T;
    static constexpr bool isMany = false;
  };
  template <typename T> struct AttributeType<std::vector<T>> {
    using type = T;
    static constexpr bool isMany = true;
  };
  template <typename T> struct AttributeType<Reference<T>> {
    using type = std::string;
    static constexpr bool isMany = false;
  };
  template <typename T> struct AttributeType<std::vector<std::shared_ptr<T>>> {
    using type = T;
    static constexpr bool isMany = true;
  };

  using AttrType = AttributeType<Type>::type;
  static constexpr bool IsMany = AttributeType<Type>::isMany;
};

template <auto Member>
using ClassType = typename MemberTraits<Member>::ClassType;

template <auto Member> using AttrType = typename MemberTraits<Member>::AttrType;
template <auto Member>
static constexpr bool IsMany = MemberTraits<Member>::IsMany;

// Generic
template <typename T> struct AssignmentHelper {
  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, T Base::*member, U &&value) const {

    if constexpr (std::is_convertible_v<U, T>) {
      node->*member = std::forward<U>(value);
    } else if constexpr (std::is_constructible_v<T, U>) {
      node->*member = T{std::forward<U>(value)};
    } else {
      static_assert(false, "not convertible or constructible");
    }
    // set the container
    if constexpr (std::derived_from<T, AstNode>) {
      (node->*member).setContainer(node, member);
    }
  }
};

template <typename T> struct AssignmentHelper<Reference<T>> {
  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, Reference<T> Base::*member, U &&value) const {
    node->*member = std::move(Reference<T>{std::forward<U>(value)});
    node->addReference(member);
  }
};
template <typename T> struct AssignmentHelper<std::vector<Reference<T>>> {
  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, std::vector<Reference<T>> Base::*member,
                  U &&value) const {
    auto index = (node->*member).size();
    (node->*member).emplace_back(Reference<T>{std::forward<U>(value)});
    node->addReference(member, index);
  }
};

template <typename T> struct AssignmentHelper<std::shared_ptr<T>> {
  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> &&
             std::derived_from<Node, Base> && std::derived_from<U, T>
  void operator()(Node *node, std::shared_ptr<T> Base::*member,
                  std::shared_ptr<U> &&value) const {

    node->*member = std::move(value);
    // set the container
    if constexpr (std::derived_from<T, AstNode>) {
      (node->*member)->setContainer(node, member);
    }
  }

  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, std::shared_ptr<T> Base::*member,
                  U &&value) const {
    node->*member = std::make_shared<U>(std::forward<U>(value));
    // set the container
    if constexpr (std::derived_from<T, AstNode>) {
      (node->*member)->setContainer(node, member);
    }
  }
};

template <typename T> struct AssignmentHelper<std::vector<T>> {
  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, std::vector<T> Base::*member, U &&value) const {

    static_assert(!std::derived_from<T, AstNode>,
                  "An AstNode must be stored in a std::shared_ptr");
    if constexpr (std::is_convertible_v<U, T>) {
      (node->*member).emplace_back(std::forward<U>(value));
    } else if constexpr (std::is_constructible_v<T, U>) {
      (node->*member).emplace_back(T{std::forward<U>(value)});
    } else {
      static_assert(false, "not convertible or constructible");
    }
  }
};

template <typename T> struct AssignmentHelper<std::vector<std::shared_ptr<T>>> {
  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> &&
             std::derived_from<Node, Base> && std::derived_from<U, T>
  void operator()(Node *node, std::vector<std::shared_ptr<T>> Base::*member,
                  std::shared_ptr<U> &&value) const {
    if constexpr (std::derived_from<T, AstNode>) {
      value->setContainer(node, member, (node->*member).size());
    }
    (node->*member).emplace_back(std::move(value));
  }

  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> &&
             std::derived_from<Node, Base> && std::derived_from<U, T>
  void operator()(Node *node, std::vector<std::shared_ptr<T>> Base::*member,
                  U &&value) const {
    auto ptr = std::make_shared<U>(std::forward<U>(value));
    if constexpr (std::derived_from<U, AstNode>) {
      ptr->setContainer(node, member, (node->*member).size());
    }
    (node->*member).emplace_back(std::move(ptr));
  }
};

} // namespace pegium::parser::helpers
