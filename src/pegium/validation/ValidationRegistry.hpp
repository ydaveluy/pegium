#pragma once

#include <concepts>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

#include <pegium/services/JsonValue.hpp>
#include <pegium/services/Diagnostic.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/utils/Cancellation.hpp>
#include <pegium/validation/ValidationAcceptor.hpp>
#include <pegium/utils/Stream.hpp>

namespace pegium::validation {

namespace detail {

template <typename> struct ValidationCheckMethodTraits;

template <typename Class, typename Node>
struct ValidationCheckMethodTraits<
    void (Class::*)(const Node &, const ValidationAcceptor &)> {
  using ClassType = Class;
  using NodeType = Node;
};

template <typename Class, typename Node>
struct ValidationCheckMethodTraits<
    void (Class::*)(const Node &, const ValidationAcceptor &) const> {
  using ClassType = Class;
  using NodeType = Node;
};

template <auto Method>
using ValidationCheckMethodClass =
    typename ValidationCheckMethodTraits<decltype(Method)>::ClassType;

template <auto Method>
using ValidationCheckMethodNode =
    typename ValidationCheckMethodTraits<decltype(Method)>::NodeType;

template <auto Method, typename Object>
concept BoundValidationCheckMethod =
    requires { typename ValidationCheckMethodTraits<decltype(Method)>; } &&
    std::derived_from<std::remove_cvref_t<Object>,
                      ValidationCheckMethodClass<Method>>;

} // namespace detail

inline constexpr std::string_view kFastValidationCategory = "fast";
inline constexpr std::string_view kSlowValidationCategory = "slow";
inline constexpr std::string_view kBuiltInValidationCategory = "built-in";

using ValidationCheck =
    std::function<void(const AstNode &, const ValidationAcceptor &)>;
using ValidationPreparation =
    std::function<void(const AstNode &, const ValidationAcceptor &,
                       std::span<const std::string>,
                       const utils::CancellationToken &)>;

class ValidationRegistry {
public:
  using NodeMatcher = std::function<bool(const AstNode &)>;
  using MatchedValidationCheck =
      std::function<void(const AstNode &, const ValidationAcceptor &)>;
  struct ValidationCheckRegistration {
    NodeMatcher matcher;
    ValidationCheck check;
    MatchedValidationCheck matchedCheck;
  };

  template <typename Node>
  using TypedValidationCheck =
      std::function<void(const Node &, const ValidationAcceptor &)>;

  virtual ~ValidationRegistry() noexcept = default;

  template <typename Node> void registerCheck(
      TypedValidationCheck<Node> check,
      std::string_view category = kFastValidationCategory) {
    if (!check) {
      return;
    }

    const auto registration = makeValidationCheck<Node>(std::move(check));
    registerMatcher(registration.matcher, registration.check,
                    registration.matchedCheck, category);
  }

  template <typename Node>
  [[nodiscard]] static ValidationCheckRegistration
  makeValidationCheck(TypedValidationCheck<Node> check) {
    auto sharedCheck =
        std::make_shared<TypedValidationCheck<Node>>(std::move(check));
    return ValidationCheckRegistration{
        .matcher = [](const AstNode &node) {
          return dynamic_cast<const Node *>(&node) != nullptr;
        },
        .check = [sharedCheck](const AstNode &node,
                               const ValidationAcceptor &acceptor) {
          if (const auto *typedNode = dynamic_cast<const Node *>(&node)) {
            (*sharedCheck)(*typedNode, acceptor);
          }
        },
        .matchedCheck = [sharedCheck](const AstNode &node,
                                      const ValidationAcceptor &acceptor) {
          (*sharedCheck)(static_cast<const Node &>(node), acceptor);
        }};
  }

  template <auto Method, typename Object>
    requires detail::BoundValidationCheckMethod<Method, Object> &&
             std::copy_constructible<std::remove_cvref_t<Object>>
  [[nodiscard]] static ValidationCheckRegistration
  makeValidationCheck(Object &&object) {
    using Node =
        std::remove_cvref_t<detail::ValidationCheckMethodNode<Method>>;
    using StoredObject = std::remove_cvref_t<Object>;
    return makeValidationCheck<Node>(
        [storedObject = StoredObject(std::forward<Object>(object))](
            const Node &node, const ValidationAcceptor &acceptor) {
          std::invoke(Method, storedObject, node, acceptor);
        });
  }

  void registerChecks(
      std::initializer_list<ValidationCheckRegistration> checks,
      std::string_view category = kFastValidationCategory) {
    for (const auto &check : checks) {
      registerMatcher(check.matcher, check.check, check.matchedCheck, category);
    }
  }

  virtual void registerCheck(
      ValidationCheck check,
      std::string_view category = kFastValidationCategory) = 0;
  virtual void registerCheck(
      std::type_index nodeType, ValidationCheck check,
      std::string_view category = kFastValidationCategory) = 0;

  [[nodiscard]] virtual utils::stream<ValidationCheck>
  getChecks(const AstNode &node,
            std::span<const std::string> categories = {}) const = 0;

  virtual void runChecks(const AstNode &node,
                         std::span<const std::string> categories,
                         const ValidationAcceptor &acceptor) const {
    auto checks = getChecks(node, categories);
    for (const auto &check : checks) {
      check(node, acceptor);
    }
  }

  [[nodiscard]] virtual std::vector<std::string>
  getAllValidationCategories() const = 0;

  virtual void registerBeforeDocument(ValidationPreparation check) = 0;
  virtual void registerAfterDocument(ValidationPreparation check) = 0;

  [[nodiscard]] virtual std::span<const ValidationPreparation>
  checksBefore() const noexcept = 0;
  [[nodiscard]] virtual std::span<const ValidationPreparation>
  checksAfter() const noexcept = 0;

  virtual void clear() = 0;

private:
  virtual void registerMatcher(NodeMatcher matcher, ValidationCheck check,
                               MatchedValidationCheck matchedCheck,
                               std::string_view category) = 0;
};

} // namespace pegium::validation
