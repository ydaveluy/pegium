#pragma once

/// Helper traits and utilities for assigning parser results into AST members.

#include <cassert>
#include <memory>
#include <optional>
#include <pegium/core/parser/OrderedChoice.hpp>
#include <pegium/core/parser/ValueBuildContext.hpp>
#include <pegium/core/references/Linker.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace pegium::parser::helpers {

template <auto Member> struct MemberTraits;

template <typename Class, typename Type, Type Class::*Member>
struct MemberTraits<Member> {
  using ClassType = Class;
  using MemberType = Type;

  template <typename T> struct AttributeType {
    using type = T;
    static constexpr bool isMany = false;
  };
  template <typename T> struct AttributeType<std::unique_ptr<T>> {
    using type = T;
    static constexpr bool isMany = false;
  };
  template <typename T> struct AttributeType<std::vector<T>> {
    using type = T;
    static constexpr bool isMany = true;
  };
  template <typename T> struct AttributeType<std::optional<T>> : AttributeType<T> {};
  template <typename T> struct AttributeType<Reference<T>> {
    using type = std::string;
    static constexpr bool isMany = false;
  };
  template <typename T> struct AttributeType<MultiReference<T>> {
    using type = std::string;
    static constexpr bool isMany = false;
  };
  template <typename T> struct AttributeType<std::vector<std::unique_ptr<T>>> {
    using type = T;
    static constexpr bool isMany = true;
  };

  using AttrType = AttributeType<Type>::type;
  static constexpr bool IsMany = AttributeType<Type>::isMany;
};

template <auto Member>
using ClassType = typename MemberTraits<Member>::ClassType;
template <auto Member>
using MemberType = typename MemberTraits<Member>::MemberType;

template <auto Member> using AttrType = typename MemberTraits<Member>::AttrType;
template <auto Member>
static constexpr bool IsMany = MemberTraits<Member>::IsMany;

inline void register_handle(const ValueBuildContext &context,
                            const ReferenceHandle &handle) {
  if (context.references != nullptr) {
    assert(handle);
    context.references->push_back(handle);
  }
}

template <typename T, typename Ref, typename Node>
void initialize_reference(Ref &reference, Node &node, std::string refText,
                          std::optional<CstNodeView> refNode,
                          const ValueBuildContext &context) {
  assert(context.linker != nullptr);
  assert(context.assignment != nullptr);
  assert(context.assignment->isReference());
  assert(context.assignment->getType() == std::type_index(typeid(T)));
  reference.initialize(node, std::move(refText), std::move(refNode),
                       *context.assignment, *context.linker);
}

template <typename U>
[[nodiscard]] std::string make_reference_text(U &&value) {
  using Value = std::remove_cvref_t<U>;
  if constexpr (std::same_as<Value, std::string>) {
    return std::forward<U>(value);
  } else if constexpr (std::same_as<Value, std::string_view>) {
    return std::string(value);
  } else if constexpr (std::same_as<Value, const char *>) {
    return std::string(value);
  } else {
    return std::string{std::forward<U>(value)};
  }
}

template <typename T>
[[nodiscard]] std::string make_reference_text(const Reference<T> &reference) {
  return reference.getRefText();
}

template <typename T>
[[nodiscard]] std::string
make_reference_text(const MultiReference<T> &reference) {
  return reference.getRefText();
}

template <typename T, typename Ref, typename Node, typename U>
void initialize_reference_from_value(Ref &reference, Node &node, U &&value,
                                     std::optional<CstNodeView> refNode,
                                     const ValueBuildContext &context) {
  initialize_reference<T>(reference, node, make_reference_text(std::forward<U>(value)),
                          std::move(refNode), context);
}

// Generic
template <typename T> struct AssignmentHelper {
  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, T Base::*member, U &&value,
                  const ValueBuildContext &context) const {

    if constexpr (std::is_convertible_v<U, T>) {
      node->*member = std::forward<U>(value);
    } else if constexpr (std::is_constructible_v<T, U>) {
      node->*member = T{std::forward<U>(value)};
    } else {
      static_assert(false, "not convertible or constructible");
    }
    // set the container
    if constexpr (std::derived_from<T, AstNode>) {
      (node->*member).attachToContainer(*node, context.property);
    }
  }
};

template <typename T> struct AssignmentHelper<Reference<T>> {
  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, Reference<T> Base::*member, U &&value,
                  const ValueBuildContext &context) const {
    initialize_reference_from_value<T>(node->*member, *node,
                                       std::forward<U>(value), std::nullopt,
                                       context);
    helpers::register_handle(context, ReferenceHandle::direct(&(node->*member)));
  }

  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, Reference<T> Base::*member, U &&value,
                  const CstNodeView &sourceNode,
                  const ValueBuildContext &context) const {
    initialize_reference_from_value<T>(node->*member, *node,
                                       std::forward<U>(value), sourceNode,
                                       context);
    helpers::register_handle(context, ReferenceHandle::direct(&(node->*member)));
  }
};

template <typename T> struct AssignmentHelper<MultiReference<T>> {
  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, MultiReference<T> Base::*member, U &&value,
                  const ValueBuildContext &context) const {
    initialize_reference_from_value<T>(node->*member, *node,
                                       std::forward<U>(value), std::nullopt,
                                       context);
    helpers::register_handle(context, ReferenceHandle::direct(&(node->*member)));
  }

  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, MultiReference<T> Base::*member, U &&value,
                  const CstNodeView &sourceNode,
                  const ValueBuildContext &context) const {
    initialize_reference_from_value<T>(node->*member, *node,
                                       std::forward<U>(value), sourceNode,
                                       context);
    helpers::register_handle(context, ReferenceHandle::direct(&(node->*member)));
  }
};
template <typename T> struct AssignmentHelper<std::vector<Reference<T>>> {
  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, std::vector<Reference<T>> Base::*member,
                  U &&value, const ValueBuildContext &context) const {
    auto &referencesVector = node->*member;
    referencesVector.emplace_back();
    const auto index = referencesVector.size() - 1;
    initialize_reference_from_value<T>(referencesVector.back(), *node,
                                       std::forward<U>(value), std::nullopt,
                                       context);
    helpers::register_handle(context,
                             ReferenceHandle::indexed(&referencesVector, index));
  }

  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, std::vector<Reference<T>> Base::*member,
                  U &&value, const CstNodeView &sourceNode,
                  const ValueBuildContext &context) const {
    auto &referencesVector = node->*member;
    referencesVector.emplace_back();
    const auto index = referencesVector.size() - 1;
    initialize_reference_from_value<T>(referencesVector.back(), *node,
                                       std::forward<U>(value), sourceNode,
                                       context);
    helpers::register_handle(context,
                             ReferenceHandle::indexed(&referencesVector, index));
  }
};

template <typename T>
struct AssignmentHelper<std::optional<Reference<T>>> {
  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, std::optional<Reference<T>> Base::*member,
                  U &&value, const ValueBuildContext &context) const {
    auto &target = node->*member;
    target.emplace();
    if constexpr (std::same_as<std::remove_cvref_t<U>, Reference<T>>) {
      const auto refNode = value.getRefNode();
      initialize_reference_from_value<T>(*target, *node, std::forward<U>(value),
                                         refNode, context);
    } else {
      initialize_reference_from_value<T>(*target, *node, std::forward<U>(value),
                                         std::nullopt, context);
    }
    helpers::register_handle(context, ReferenceHandle::optional(&(node->*member)));
  }

  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, std::optional<Reference<T>> Base::*member,
                  U &&value, const CstNodeView &sourceNode,
                  const ValueBuildContext &context) const {
    auto &target = node->*member;
    target.emplace();
    initialize_reference_from_value<T>(*target, *node, std::forward<U>(value),
                                       sourceNode, context);
    helpers::register_handle(context, ReferenceHandle::optional(&(node->*member)));
  }
};

template <typename T>
struct AssignmentHelper<std::optional<MultiReference<T>>> {
  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, std::optional<MultiReference<T>> Base::*member,
                  U &&value, const ValueBuildContext &context) const {
    auto &target = node->*member;
    target.emplace();
    if constexpr (std::same_as<std::remove_cvref_t<U>, MultiReference<T>>) {
      const auto refNode = value.getRefNode();
      initialize_reference_from_value<T>(*target, *node, std::forward<U>(value),
                                         refNode, context);
    } else {
      initialize_reference_from_value<T>(*target, *node, std::forward<U>(value),
                                         std::nullopt, context);
    }
    helpers::register_handle(context, ReferenceHandle::optional(&(node->*member)));
  }

  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, std::optional<MultiReference<T>> Base::*member,
                  U &&value, const CstNodeView &sourceNode,
                  const ValueBuildContext &context) const {
    auto &target = node->*member;
    target.emplace();
    initialize_reference_from_value<T>(*target, *node, std::forward<U>(value),
                                       sourceNode, context);
    helpers::register_handle(context, ReferenceHandle::optional(&(node->*member)));
  }
};

template <typename T> struct AssignmentHelper<std::unique_ptr<T>> {
  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> &&
             std::derived_from<Node, Base> && std::derived_from<U, T>
  void operator()(Node *node, std::unique_ptr<T> Base::*member,
                  std::unique_ptr<U> &&value,
                  const ValueBuildContext &context) const {
    auto &target = node->*member;
    target = std::move(value);
    if constexpr (std::derived_from<T, AstNode>) {
      if (target) {
        target->attachToContainer(*node, context.property);
      }
    }
  }

  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, std::unique_ptr<T> Base::*member,
                  U &&value, const ValueBuildContext &context) const {
    auto &target = node->*member;
    target = std::make_unique<U>(std::forward<U>(value));
    if constexpr (std::derived_from<T, AstNode>) {
      target->attachToContainer(*node, context.property);
    }
  }
};

template <typename T> struct AssignmentHelper<std::vector<T>> {
  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void operator()(Node *node, std::vector<T> Base::*member, U &&value,
                  const ValueBuildContext &) const {

    static_assert(!std::derived_from<T, AstNode>,
                  "An AstNode must be stored in a std::unique_ptr");
    if constexpr (std::is_convertible_v<U, T>) {
      (node->*member).emplace_back(std::forward<U>(value));
    } else if constexpr (std::is_constructible_v<T, U>) {
      (node->*member).emplace_back(T{std::forward<U>(value)});
    } else {
      static_assert(false, "not convertible or constructible");
    }
  }
};

template <typename T> struct AssignmentHelper<std::vector<std::unique_ptr<T>>> {
  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> &&
             std::derived_from<Node, Base> && std::derived_from<U, T>
  void operator()(Node *node, std::vector<std::unique_ptr<T>> Base::*member,
                  std::unique_ptr<U> &&value,
                  const ValueBuildContext &context) const {
    auto &target = node->*member;
    if constexpr (std::derived_from<T, AstNode>) {
      value->attachToContainer(*node, context.property, target.size());
    }
    target.emplace_back(std::move(value));
  }

  template <typename Node, typename Base, typename U>
    requires std::derived_from<Node, AstNode> &&
             std::derived_from<Node, Base> && std::derived_from<U, T>
  void operator()(Node *node, std::vector<std::unique_ptr<T>> Base::*member,
                  U &&value, const ValueBuildContext &context) const {
    auto &target = node->*member;
    auto ptr = std::make_unique<U>(std::forward<U>(value));
    if constexpr (std::derived_from<U, AstNode>) {
      ptr->attachToContainer(*node, context.property, target.size());
    }
    target.emplace_back(std::move(ptr));
  }
};

template <auto feature, typename Element, typename = void>
struct IsValidAssignmentImpl : std::false_type {};

template <auto feature, typename Element>
struct IsValidAssignmentImpl<feature, Element,
                             std::void_t<typename Element::type>>
    : std::bool_constant<
          (
              // If the Element type is an AstNode
              std::derived_from<helpers::AttrType<feature>, AstNode> &&

              // Check that the element type is convertible to AttrType
              std::derived_from<typename Element::type,
                                helpers::AttrType<feature>>) ||
          // If the element Type is not an AstType
          (
              // Check that the type is convertible to AttrType
              std::convertible_to<typename Element::type,
                                  helpers::AttrType<feature>> ||
              // or reference-like attributes can be assigned from string-like values
              (pegium::is_reference_v<helpers::AttrType<feature>> &&
               (std::convertible_to<typename Element::type, std::string> ||
                std::convertible_to<typename Element::type,
                                    std::string_view>)) ||
              // or AttrType constructible from the given type
              std::constructible_from<helpers::AttrType<feature>,
                                      typename Element::type> ||
              // or AttrType constructible from a shared_ptr of the given type
              std::constructible_from<
                  helpers::AttrType<feature>,
                  std::unique_ptr<typename Element::type>>)> {};

template <auto feature, typename... Element>
struct IsValidAssignmentImpl<feature, OrderedChoice<Element...>, void>
    : std::bool_constant<(IsValidAssignmentImpl<feature,
                                               std::remove_cvref_t<Element>>::value &&
                          ...)> {};

template <auto feature, typename Element>
struct IsValidAssignment
    : IsValidAssignmentImpl<feature, std::remove_cvref_t<Element>> {};

} // namespace pegium::parser::helpers
