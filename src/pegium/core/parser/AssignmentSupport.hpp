#pragma once

/// Assignment implementation machinery used by parser-side assignment expressions.

#include <algorithm>
#include <cassert>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <type_traits>
#include <variant>
#include <vector>

#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/grammar/FeatureValue.hpp>
#include <pegium/core/parser/AssignmentHelpers.hpp>
#include <pegium/core/parser/FeatureValueSupport.hpp>
#include <pegium/core/parser/Introspection.hpp>
#include <pegium/core/parser/NodeParseHelpers.hpp>
#include <pegium/core/parser/OrderedChoice.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/RawValueTraits.hpp>
#include <pegium/core/parser/AstReflectionBootstrap.hpp>
#include <pegium/core/parser/ValueBuildContext.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/core/syntax-tree/Reference.hpp>

namespace pegium::parser::detail {

template <typename Member> struct AssignmentReferenceMemberInfo {
  static constexpr bool isReference = false;
  static constexpr bool isMulti = false;
  using TargetType = void;
};

template <typename T> struct AssignmentReferenceMemberInfo<Reference<T>> {
  static constexpr bool isReference = true;
  static constexpr bool isMulti = false;
  using TargetType = T;
};

template <typename T> struct AssignmentReferenceMemberInfo<MultiReference<T>> {
  static constexpr bool isReference = true;
  static constexpr bool isMulti = true;
  using TargetType = T;
};

template <typename T>
struct AssignmentReferenceMemberInfo<std::vector<Reference<T>>> {
  static constexpr bool isReference = true;
  static constexpr bool isMulti = true;
  using TargetType = T;
};

template <typename T>
struct AssignmentReferenceMemberInfo<std::optional<T>>
    : AssignmentReferenceMemberInfo<T> {};

template <auto feature> struct AssignmentReferenceSupport {
  using ReferenceInfo =
      AssignmentReferenceMemberInfo<std::remove_cvref_t<helpers::MemberType<feature>>>;

  static constexpr bool isReference = ReferenceInfo::isReference;
  static constexpr bool isMultiReference = ReferenceInfo::isMulti;

  [[nodiscard]] static const AstNodeTypeInfo *
  getReferenceTargetTypeInfo() noexcept {
    if constexpr (isReference) {
      return std::addressof(
          ast_node_type_info<typename ReferenceInfo::TargetType>());
    }
    return nullptr;
  }
};

template <typename T> struct AssignmentIsVariant : std::false_type {};
template <typename... Ts>
struct AssignmentIsVariant<std::variant<Ts...>> : std::true_type {};

template <typename E, typename = void> struct AssignmentElementValueType {
  using type = void;
};

template <typename E>
struct AssignmentElementValueType<
    E, std::void_t<typename std::remove_cvref_t<E>::type>> {
  using type = typename std::remove_cvref_t<E>::type;
};

template <typename E>
using AssignmentElementValueType_t = typename AssignmentElementValueType<E>::type;

template <typename T> struct AssignmentIsUniquePtr : std::false_type {};
template <typename T>
struct AssignmentIsUniquePtr<std::unique_ptr<T>> : std::true_type {
  using type = T;
};

template <typename T> struct AssignmentIsVectorUniquePtr : std::false_type {};
template <typename T>
struct AssignmentIsVectorUniquePtr<std::vector<std::unique_ptr<T>>>
    : std::true_type {
  using type = T;
};

template <typename T> struct AssignmentIsOptional : std::false_type {};
template <typename T>
struct AssignmentIsOptional<std::optional<T>> : std::true_type {
  using type = T;
};

enum class OrderedChoiceAssignStatus { Assigned, AssignFailed, NoSelectable };

template <auto feature>
[[nodiscard]] inline std::runtime_error
ordered_choice_no_selectable_error() {
  return std::runtime_error("OrderedChoice has no assignable selected value for " +
                            std::string(detail::member_name_v<feature>));
}

template <auto feature>
[[nodiscard]] inline std::runtime_error
ordered_choice_no_selectable_error(const CstNodeView &node) {
  std::string message =
      "OrderedChoice has no assignable selected value for " +
      std::string(detail::member_name_v<feature>) +
      " nodeText=\"" + std::string(node.getText()) + "\"" +
      " recovered=" + (node.isRecovered() ? "true" : "false") +
      " leaf=" + (node.isLeaf() ? "true" : "false");
  std::size_t childIndex = 0;
  for (const auto child : node) {
    std::ostringstream grammarStream;
    grammarStream << *child.getGrammarElement();
    message += " child[" + std::to_string(childIndex++) + "]={text=\"" +
               std::string(child.getText()) + "\", grammar=\"" +
               grammarStream.str() + "\"";
    message += ", hidden=";
    message += child.isHidden() ? "true" : "false";
    message += ", recovered=";
    message += child.isRecovered() ? "true" : "false";
    message += ", leaf=";
    message += child.isLeaf() ? "true" : "false";
    message += "}";
  }
  return std::runtime_error(std::move(message));
}

template <auto feature>
[[nodiscard]] inline std::runtime_error
ordered_choice_not_assignable_error() {
  return std::runtime_error("OrderedChoice value is not assignable to " +
                            std::string(detail::member_name_v<feature>));
}

template <auto feature>
[[nodiscard]] inline std::runtime_error assignment_not_assignable_error() {
  return std::runtime_error("Assignment value is not assignable to " +
                            std::string(detail::member_name_v<feature>));
}

template <auto feature> struct AssignmentConversionSupport {
public:
  template <typename ClassType, typename AttrType, typename Value>
  static bool assign_reference_target(ClassType *astNode,
                                      AttrType ClassType::*member,
                                      Value &&value,
                                      const ValueBuildContext &context,
                                      const CstNodeView *sourceNode) {
    using RawValueType = std::remove_cvref_t<Value>;
    if constexpr (pegium::is_reference_v<AttrType>) {
      if constexpr (std::same_as<RawValueType, std::string> ||
                    std::same_as<RawValueType, std::string_view>) {
        auto refText = std::string{std::forward<Value>(value)};
        if (sourceNode != nullptr) {
          helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                                std::move(refText), *sourceNode,
                                                context);
        } else {
          helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                                std::move(refText), context);
        }
        return true;
      }
      return false;
    }
    return false;
  }

  template <typename ClassType, typename AttrType, typename Value>
  static bool assign_optional_target(ClassType *astNode,
                                     AttrType ClassType::*member, Value &&value,
                                     const ValueBuildContext &context) {
    using RawValueType = std::remove_cvref_t<Value>;
    if constexpr (AssignmentIsOptional<AttrType>::value) {
      using OptionalValueType = typename AssignmentIsOptional<AttrType>::type;
      if constexpr (pegium::is_reference_v<OptionalValueType>) {
        if constexpr (std::same_as<RawValueType, std::string> ||
                      std::same_as<RawValueType, std::string_view>) {
          helpers::AssignmentHelper<AttrType>{}(
              astNode, member, std::string{std::forward<Value>(value)},
              context);
          return true;
        }
        return false;
      }

      if constexpr (std::same_as<RawValueType, OptionalValueType>) {
        if constexpr (std::same_as<AttrType, std::optional<OptionalValueType>> &&
                      !std::derived_from<OptionalValueType, AstNode>) {
          (astNode->*member) = std::forward<Value>(value);
          return true;
        }
        helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                              std::forward<Value>(value),
                                              context);
        return true;
      } else if constexpr (std::is_enum_v<OptionalValueType> &&
                           std::is_integral_v<RawValueType>) {
        helpers::AssignmentHelper<AttrType>{}(
            astNode, member, static_cast<OptionalValueType>(value), context);
        return true;
      } else if constexpr (std::same_as<OptionalValueType, std::string> &&
                           std::same_as<RawValueType, std::string_view>) {
        if constexpr (std::same_as<AttrType, std::optional<std::string>>) {
          auto &target = astNode->*member;
          target.emplace();
          target->assign(value.data(), value.size());
          return true;
        }
        helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                              std::string{value}, context);
        return true;
      }
      return false;
    }
    return false;
  }

  template <typename ClassType, typename AttrType, typename Value>
  static bool assign_pointer_target(ClassType *astNode,
                                    AttrType ClassType::*member, Value &&value,
                                    const ValueBuildContext &context) {
    using RawValueType = std::remove_cvref_t<Value>;
    if constexpr (AssignmentIsUniquePtr<AttrType>::value ||
                  AssignmentIsVectorUniquePtr<AttrType>::value) {
      using PointeeType =
          typename std::conditional_t<AssignmentIsUniquePtr<AttrType>::value,
                                      AssignmentIsUniquePtr<AttrType>,
                                      AssignmentIsVectorUniquePtr<AttrType>>::type;

      if constexpr (AssignmentIsUniquePtr<RawValueType>::value) {
        using ValuePointeeType =
            typename AssignmentIsUniquePtr<RawValueType>::type;
        if constexpr (std::derived_from<ValuePointeeType, PointeeType>) {
          auto ptr = std::remove_cvref_t<Value>{std::forward<Value>(value)};
          helpers::AssignmentHelper<AttrType>{}(astNode, member, std::move(ptr),
                                                context);
          return true;
        } else if constexpr (std::derived_from<ValuePointeeType, AstNode> &&
                             std::derived_from<PointeeType, AstNode>) {
          auto *castedPtr =
              value ? dynamic_cast<PointeeType *>(value.get()) : nullptr;
          if (!castedPtr) {
            return false;
          }
          value.release();
          std::unique_ptr<PointeeType> casted(castedPtr);
          helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                                std::move(casted), context);
          return true;
        }
      } else if constexpr (std::derived_from<RawValueType, PointeeType>) {
        using NormalizedValue = std::remove_cvref_t<Value>;
        helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                              NormalizedValue{
                                                  std::forward<Value>(value)},
                                              context);
        return true;
      }
      return false;
    }
    return false;
  }

  template <typename ClassType, typename AttrType, typename Value>
  static bool assign_scalar_target(ClassType *astNode,
                                   AttrType ClassType::*member, Value &&value,
                                   const ValueBuildContext &context) {
    using TargetValueType = helpers::AttrType<feature>;
    using RawValueType = std::remove_cvref_t<Value>;
    if constexpr (std::same_as<RawValueType, TargetValueType>) {
      if constexpr (std::same_as<AttrType, TargetValueType> &&
                    !std::derived_from<TargetValueType, AstNode>) {
        astNode->*member = std::forward<Value>(value);
      } else {
        using NormalizedValue = std::remove_cvref_t<Value>;
        helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                              NormalizedValue{
                                                  std::forward<Value>(value)},
                                              context);
      }
      return true;
    } else if constexpr (std::is_enum_v<TargetValueType> &&
                         std::is_integral_v<RawValueType>) {
      helpers::AssignmentHelper<AttrType>{}(
          astNode, member, static_cast<TargetValueType>(value), context);
      return true;
    } else if constexpr (std::same_as<TargetValueType, std::string> &&
                         std::same_as<RawValueType, std::string_view>) {
      if constexpr (std::same_as<AttrType, std::string>) {
        (astNode->*member).assign(value.data(), value.size());
      } else {
        helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                              std::string{value}, context);
      }
      return true;
    } else if constexpr (std::same_as<TargetValueType, std::nullptr_t> &&
                         std::same_as<RawValueType, std::nullptr_t>) {
      helpers::AssignmentHelper<AttrType>{}(astNode, member, nullptr, context);
      return true;
    }
    return false;
  }

  template <typename ClassType, typename AttrType, typename Value>
  static bool assign_direct_strict(ClassType *astNode,
                                   AttrType ClassType::*member, Value &&value,
                                   const ValueBuildContext &context,
                                   const CstNodeView *sourceNode = nullptr) {
    if constexpr (pegium::is_reference_v<AttrType>) {
      return assign_reference_target(astNode, member, std::forward<Value>(value),
                                     context, sourceNode);
    } else if constexpr (AssignmentIsOptional<AttrType>::value) {
      return assign_optional_target(astNode, member, std::forward<Value>(value),
                                    context);
    } else if constexpr (AssignmentIsUniquePtr<AttrType>::value ||
                         AssignmentIsVectorUniquePtr<AttrType>::value) {
      return assign_pointer_target(astNode, member, std::forward<Value>(value),
                                   context);
    } else {
      return assign_scalar_target(astNode, member, std::forward<Value>(value),
                                  context);
    }
  }

  template <typename VariantType, typename Value, std::size_t I = 0>
  static bool assign_variant_exact(VariantType &target, Value &&value) {
    if constexpr (I == std::variant_size_v<VariantType>) {
      return false;
    } else {
      using Alternative = std::variant_alternative_t<I, VariantType>;
      if constexpr (std::same_as<Alternative, std::remove_cvref_t<Value>>) {
        target = std::forward<Value>(value);
        return true;
      } else if constexpr (std::is_enum_v<Alternative> &&
                           std::is_integral_v<std::remove_cvref_t<Value>>) {
        target = static_cast<Alternative>(value);
        return true;
      }
      return assign_variant_exact<VariantType, Value, I + 1>(
          target, std::forward<Value>(value));
    }
  }

  template <typename ClassType, typename VariantType, typename Value,
            std::size_t I = 0>
  static bool assign_variant_from_ast_ptr(ClassType *astNode,
                                          VariantType &target, Value &&value,
                                          const ValueBuildContext &context) {
    if constexpr (I == std::variant_size_v<VariantType>) {
      return false;
    } else {
      using Alternative = std::variant_alternative_t<I, VariantType>;
      if constexpr (AssignmentIsUniquePtr<Alternative>::value) {
        using AlternativePointee =
            typename AssignmentIsUniquePtr<Alternative>::type;
        if constexpr (std::derived_from<AlternativePointee, AstNode>) {
          auto *castedPtr =
              value ? dynamic_cast<AlternativePointee *>(value.get()) : nullptr;
          if (castedPtr) {
            value.release();
            castedPtr->attachToContainer(*astNode, context.property);
            target = Alternative(castedPtr);
            return true;
          }
        }
      }
      return assign_variant_from_ast_ptr<ClassType, VariantType, Value, I + 1>(
          astNode, target, std::forward<Value>(value), context);
    }
  }

  template <typename ClassType, typename AttrType, typename RawUniquePtr>
  static bool assign_ast_ptr_to_target(ClassType *astNode,
                                       AttrType ClassType::*member,
                                       RawUniquePtr &&rawValue,
                                       const ValueBuildContext &context) {
    using RawUniqueType = std::remove_cvref_t<RawUniquePtr>;
    using RawPointeeType = typename AssignmentIsUniquePtr<RawUniqueType>::type;

    if constexpr (AssignmentIsUniquePtr<AttrType>::value ||
                  AssignmentIsVectorUniquePtr<AttrType>::value) {
      using TargetPointeeType =
          typename std::conditional_t<AssignmentIsUniquePtr<AttrType>::value,
                                      AssignmentIsUniquePtr<AttrType>,
                                      AssignmentIsVectorUniquePtr<AttrType>>::type;

      if constexpr (std::derived_from<RawPointeeType, TargetPointeeType>) {
        auto ptr = RawUniqueType{std::forward<RawUniquePtr>(rawValue)};
        helpers::AssignmentHelper<AttrType>{}(astNode, member, std::move(ptr),
                                              context);
        return true;
      } else if constexpr (std::same_as<RawPointeeType, AstNode> &&
                           std::derived_from<TargetPointeeType, AstNode>) {
        auto *castedPtr =
            rawValue ? dynamic_cast<TargetPointeeType *>(rawValue.get()) : nullptr;
        if (!castedPtr) {
          return false;
        }
        rawValue.release();
        std::unique_ptr<TargetPointeeType> casted(castedPtr);
        helpers::AssignmentHelper<AttrType>{}(astNode, member,
                                              std::move(casted), context);
        return true;
      }
    }
    return false;
  }

  template <typename ClassType, typename AttrType, typename RawValue>
  static bool convert_raw_to_target(ClassType *astNode,
                                    AttrType ClassType::*member,
                                    RawValue &&rawValue,
                                    const ValueBuildContext &context,
                                    const CstNodeView *sourceNode = nullptr) {
    using RawType = std::remove_cvref_t<RawValue>;
    using TargetValueType = helpers::AttrType<feature>;

    if constexpr (AssignmentIsUniquePtr<RawType>::value &&
                  std::derived_from<typename AssignmentIsUniquePtr<RawType>::type,
                                    AstNode>) {
      if constexpr (AssignmentIsVariant<TargetValueType>::value) {
        TargetValueType converted{};
        if (!assign_variant_from_ast_ptr(astNode, converted,
                                         std::forward<RawValue>(rawValue),
                                         context)) {
          return false;
        }
        return assign_direct_strict(astNode, member, std::move(converted),
                                    context, sourceNode);
      }
      return assign_ast_ptr_to_target(astNode, member,
                                      std::forward<RawValue>(rawValue),
                                      context);
    }

    if constexpr (AssignmentIsVariant<TargetValueType>::value) {
      TargetValueType converted{};
      bool assigned =
          assign_variant_exact(converted, std::forward<RawValue>(rawValue));
      if constexpr (std::same_as<RawType, std::string_view>) {
        if (!assigned) {
          assigned = assign_variant_exact(converted, std::string{rawValue});
        }
      }
      if (!assigned) {
        return false;
      }
      if constexpr (std::same_as<AttrType, TargetValueType>) {
        astNode->*member = std::move(converted);
        return true;
      }
      return assign_direct_strict(astNode, member, std::move(converted), context,
                                  sourceNode);
    }

    return assign_direct_strict(astNode, member, std::forward<RawValue>(rawValue),
                                context, sourceNode);
  }
};

template <auto feature, typename Element> struct AssignmentRuntimeSupport {
private:
  using ConversionSupport = AssignmentConversionSupport<feature>;

  [[nodiscard]] static bool
  ordered_choice_contains_recovery(const CstNodeView &node) {
    if (node.isRecovered()) {
      return true;
    }
    return std::ranges::any_of(node, [](const CstNodeView &child) {
      return ordered_choice_contains_recovery(child);
    });
  }

  template <typename ClassType, typename AttrType>
  static OrderedChoiceAssignStatus
  assign_ordered_choice_value(const Element &element, ClassType *astNode,
                              AttrType ClassType::*member,
                              const CstNodeView &node,
                              const ValueBuildContext &context) {
    for (const auto view : node) {
      const auto &child = view.node();
      if (child.isHidden) {
        continue;
      }
      const auto *selectedGrammarElement = child.grammarElement;
      assert(selectedGrammarElement &&
             "OrderedChoice selected node must have a grammar element");
      OrderedChoiceAssignStatus status = OrderedChoiceAssignStatus::NoSelectable;
      const bool matched = detail::visit_selected_ordered_choice_raw_value(
          element, selectedGrammarElement, view, context,
          [&astNode, member, &context, &node, &status]<typename RawValue>(
              RawValue &&rawValue) {
            status = ConversionSupport::convert_raw_to_target(
                         astNode, member,
                         std::forward<RawValue>(rawValue), context, &node)
                         ? OrderedChoiceAssignStatus::Assigned
                         : OrderedChoiceAssignStatus::AssignFailed;
          });
      if (matched) {
        return status;
      }
    }
    return OrderedChoiceAssignStatus::NoSelectable;
  }

  template <typename ClassType, typename AttrType>
  static void execute_assignment_value(const Element &element, AstNode *current,
                                       AttrType ClassType::*,
                                       const CstNodeView &node,
                                       const ValueBuildContext &context) {
    assert(current);
    auto *astNode = static_cast<ClassType *>(current);

    if constexpr (IsOrderedChoice<Element>::value) {
      const auto status =
          assign_ordered_choice_value(element, astNode, feature, node, context);
      if (status == OrderedChoiceAssignStatus::NoSelectable) [[unlikely]] {
        if (ordered_choice_contains_recovery(node)) {
          return;
        }
        throw detail::ordered_choice_no_selectable_error<feature>(node);
      }
      if (status == OrderedChoiceAssignStatus::AssignFailed) [[unlikely]] {
        throw detail::ordered_choice_not_assignable_error<feature>();
      }
      return;
    } else if constexpr (std::is_same_v<bool, AttrType> &&
                         !std::is_same_v<bool,
                                         AssignmentElementValueType_t<Element>>) {
      helpers::AssignmentHelper<AttrType>{}(astNode, feature, true, context);
    } else {
      if (ConversionSupport::convert_raw_to_target(
              astNode, feature,
              detail::extract_raw_value(element, node, context), context,
              &node)) {
        return;
      }
      throw detail::assignment_not_assignable_error<feature>();
    }
  }

public:
  static void execute(const Element &element, AstNode *current,
                      const CstNodeView &node,
                      const ValueBuildContext &context) {
    execute_assignment_value(element, current, feature, node, context);
  }

  static grammar::FeatureValue getValue(const AstNode *current) {
    return FeatureValueSupport::template get_value<feature>(current, feature);
  }
};

template <auto feature, typename Element> struct AssignmentParseSupport {
private:
  using ReferenceSupport = AssignmentReferenceSupport<feature>;

  template <ParseModeContext Context>
  [[gnu::always_inline]] static bool
  parse_element(Context &ctx, const grammar::Assignment *assignment,
                const Element &element) {
    if constexpr (IsOrderedChoice<Element>::value) {
      return detail::parse_wrapped_node(ctx, assignment, element);
    }
    return detail::parse_overriding_first_child(ctx, assignment, element);
  }

public:
  template <ParseModeContext Context>
  [[gnu::always_inline]] static bool
  parse(Context &ctx, const grammar::Assignment *assignment,
        const Element &element) {
    if constexpr (ExpectParseModeContext<Context>) {
      if constexpr (ReferenceSupport::isReference) {
        if (ctx.reachedAnchor()) {
          ctx.addReference(assignment);
          return true;
        }
      }
      auto assignmentGuard = ctx.with_assignment(assignment);
      (void)assignmentGuard;
      return parse_element(ctx, assignment, element);
    }
    return parse_element(ctx, assignment, element);
  }
};

} // namespace pegium::parser::detail
