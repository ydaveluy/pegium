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

#include <pegium/core/services/JsonValue.hpp>
#include <pegium/core/services/Diagnostic.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/validation/ValidationAcceptor.hpp>

namespace pegium::validation {

namespace detail {

template <typename> struct ValidationCheckMethodTraits;

template <typename Class, typename Node>
struct ValidationCheckMethodTraits<
    void (Class::*)(const Node &, const ValidationAcceptor &)> {
  using ClassType = Class;
  using NodeType = Node;
  static constexpr bool acceptsCancellation = false;
};

template <typename Class, typename Node>
struct ValidationCheckMethodTraits<
    void (Class::*)(const Node &, const ValidationAcceptor &) const> {
  using ClassType = Class;
  using NodeType = Node;
  static constexpr bool acceptsCancellation = false;
};

template <typename Class, typename Node>
struct ValidationCheckMethodTraits<
    void (Class::*)(const Node &, const ValidationAcceptor &,
                    const utils::CancellationToken &)> {
  using ClassType = Class;
  using NodeType = Node;
  static constexpr bool acceptsCancellation = true;
};

template <typename Class, typename Node>
struct ValidationCheckMethodTraits<
    void (Class::*)(const Node &, const ValidationAcceptor &,
                    const utils::CancellationToken &) const> {
  using ClassType = Class;
  using NodeType = Node;
  static constexpr bool acceptsCancellation = true;
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

template <auto Method>
inline constexpr bool kValidationCheckMethodAcceptsCancellation =
    ValidationCheckMethodTraits<decltype(Method)>::acceptsCancellation;

template <typename Node, typename Check>
concept TypedValidationCheckCallable =
    std::derived_from<std::remove_cvref_t<Node>, AstNode> &&
    (std::invocable<Check &, const Node &, const ValidationAcceptor &> ||
     std::invocable<Check &, const Node &, const ValidationAcceptor &,
                    const utils::CancellationToken &>);

template <typename Check>
concept NullableValidationCallable =
    requires(const Check &check) { static_cast<bool>(check); };

template <typename Check>
[[nodiscard]] bool hasValidationCallable(const Check &check) {
  if constexpr (NullableValidationCallable<Check>) {
    return static_cast<bool>(check);
  }
  return true;
}

template <typename Node, typename Check>
void invokeValidationCheck(Check &check, const Node &node,
                           const ValidationAcceptor &acceptor,
                           const utils::CancellationToken &cancelToken) {
  if constexpr (std::invocable<Check &, const Node &,
                               const ValidationAcceptor &,
                               const utils::CancellationToken &>) {
    std::invoke(check, node, acceptor, cancelToken);
  } else {
    std::invoke(check, node, acceptor);
  }
}

} // namespace detail

/// Built-in category for inexpensive validation checks.
inline constexpr std::string_view kFastValidationCategory = "fast";
/// Built-in category for expensive validation checks.
inline constexpr std::string_view kSlowValidationCategory = "slow";
/// Built-in category for parser/linker diagnostics emitted by the runtime.
inline constexpr std::string_view kBuiltInValidationCategory = "built-in";

/// Type-erased validation check runnable on any AST node.
using ValidationCheck =
    std::function<void(const AstNode &, const ValidationAcceptor &,
                       const utils::CancellationToken &)>;
/// Type-erased validation preparation run before or after AST traversal.
using ValidationPreparation =
    std::function<void(const AstNode &, const ValidationAcceptor &,
                       std::span<const std::string>,
                       const utils::CancellationToken &)>;

/// Registry of validation checks grouped by target type and category.
class ValidationRegistry {
public:
  /// Immutable prepared snapshot of the checks currently registered.
  class PreparedChecks {
  public:
    virtual ~PreparedChecks() noexcept = default;

    virtual void run(const AstNode &node, const ValidationAcceptor &acceptor,
                     const utils::CancellationToken &cancelToken) const = 0;
  };

  /// Type-erased registration payload for one validation check.
  struct ValidationCheckRegistration {
    std::type_index targetType = std::type_index(typeid(AstNode));
    ValidationCheck check;
  };

  virtual ~ValidationRegistry() noexcept = default;

  template <typename Node, typename Check>
    requires detail::TypedValidationCheckCallable<std::remove_cvref_t<Node>,
                                                  std::remove_cvref_t<Check>>
  void registerCheck(Check &&check,
      std::string_view category = kFastValidationCategory) {
    registerTypedCheck(
        makeValidationCheck<std::remove_cvref_t<Node>>(std::forward<Check>(check)),
        category);
  }

  template <typename Node, typename Check>
    requires detail::TypedValidationCheckCallable<std::remove_cvref_t<Node>,
                                                  std::remove_cvref_t<Check>>
  [[nodiscard]] static ValidationCheckRegistration
  makeValidationCheck(Check &&check) {
    using TypedNode = std::remove_cvref_t<Node>;
    using StoredCheck = std::remove_cvref_t<Check>;
    if (!detail::hasValidationCallable(check)) {
      return {};
    }

    auto sharedCheck =
        std::make_shared<StoredCheck>(std::forward<Check>(check));
    return ValidationCheckRegistration{
        .targetType = std::type_index(typeid(TypedNode)),
        .check = [sharedCheck](const AstNode &node,
                               const ValidationAcceptor &acceptor,
                               const utils::CancellationToken &cancelToken) {
          detail::invokeValidationCheck(
              *sharedCheck, static_cast<const TypedNode &>(node), acceptor,
              cancelToken);
        }};
  }

  template <auto Method, typename Object>
    requires detail::BoundValidationCheckMethod<Method, Object> &&
             std::constructible_from<std::remove_cvref_t<Object>, Object>
  [[nodiscard]] static ValidationCheckRegistration
  makeValidationCheck(Object &&object) {
    using Node =
        std::remove_cvref_t<detail::ValidationCheckMethodNode<Method>>;
    using StoredObject = std::remove_cvref_t<Object>;
    auto sharedObject =
        std::make_shared<StoredObject>(std::forward<Object>(object));
    return makeValidationCheck<Node>(
        [sharedObject = std::move(sharedObject)](
            const Node &node, const ValidationAcceptor &acceptor,
            const utils::CancellationToken &cancelToken) {
          if constexpr (detail::kValidationCheckMethodAcceptsCancellation<
                            Method>) {
            std::invoke(Method, *sharedObject, node, acceptor, cancelToken);
          } else {
            std::invoke(Method, *sharedObject, node, acceptor);
          }
        });
  }

  void registerChecks(
      std::initializer_list<ValidationCheckRegistration> checks,
      std::string_view category = kFastValidationCategory) {
    for (const auto &check : checks) {
      registerTypedCheck(check, category);
    }
  }

  // Builds an immutable snapshot of the currently registered checks.
  // Later registrations only affect future preparations.
  [[nodiscard]] virtual std::unique_ptr<const PreparedChecks>
  prepareChecks(std::span<const std::string> categories = {}) const = 0;

  // Validation categories reflect the registry state currently available for
  // future preparations.
  [[nodiscard]] virtual std::vector<std::string>
  getAllValidationCategories() const = 0;

  virtual void registerBeforeDocument(ValidationPreparation check) = 0;
  virtual void registerAfterDocument(ValidationPreparation check) = 0;

  [[nodiscard]] virtual std::span<const ValidationPreparation>
  checksBefore() const noexcept = 0;
  [[nodiscard]] virtual std::span<const ValidationPreparation>
  checksAfter() const noexcept = 0;

private:
  virtual void registerTypedCheck(ValidationCheckRegistration registration,
                                  std::string_view category) = 0;
};

} // namespace pegium::validation
