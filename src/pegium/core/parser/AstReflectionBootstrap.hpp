#pragma once

#include <cassert>
#include <optional>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/grammar/ParserRule.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/core/syntax-tree/AstReflection.hpp>

namespace pegium::parser {

struct AstReflectionInitContext;

namespace detail {

template <typename Expr>
concept HasInitImpl =
    requires(const Expr &expression, AstReflectionInitContext &ctx) {
      expression.init_impl(ctx);
    };

[[nodiscard]] inline std::type_index invalid_type() noexcept {
  return std::type_index(typeid(void));
}

[[nodiscard]] inline bool is_valid_type(std::type_index type) noexcept {
  return type != invalid_type();
}

struct AstNodeTypeInfo {
  std::type_index type = invalid_type();
  bool (*isInstance)(const AstNode &) noexcept = nullptr;
  const AstNode &(*probe)() noexcept = nullptr;
};

struct AssignmentReflectionInfo {
  const AstNodeTypeInfo *assignedAstType = nullptr;
  const AstNodeTypeInfo *referenceTargetType = nullptr;
};

template <typename T>
[[nodiscard]] bool is_ast_instance(const AstNode &node) noexcept {
  return dynamic_cast<const T *>(&node) != nullptr;
}

template <typename T>
[[nodiscard]] const AstNode &ast_probe() noexcept {
  static const T probe{};
  return probe;
}

template <typename T>
  requires std::derived_from<T, AstNode>
[[nodiscard]] consteval auto ast_probe_fn() noexcept
    -> const AstNode &(*)() noexcept {
  if constexpr (DefaultConstructibleAstNode<T>) {
    return &ast_probe<T>;
  } else {
    return nullptr;
  }
}

/// Reflection bootstrap needs probing only for concrete produced AST nodes.
/// Abstract or non-default-constructible AST supertypes can still participate
/// in metadata as reference targets or expected assignment types because their
/// RTTI-based `isInstance(...)` check remains valid without a probe object.
template <typename T>
  requires std::derived_from<T, AstNode>
[[nodiscard]] inline const AstNodeTypeInfo &ast_node_type_info() noexcept {
  static const AstNodeTypeInfo info{
      .type = std::type_index(typeid(T)),
      .isInstance = &is_ast_instance<T>,
      .probe = ast_probe_fn<T>(),
  };
  return info;
}

struct VisitedKey {
  const grammar::AbstractElement *element = nullptr;
  std::type_index expectedType = invalid_type();

  [[nodiscard]] bool operator==(const VisitedKey &) const noexcept = default;
};

struct VisitedKeyHash {
  [[nodiscard]] std::size_t operator()(const VisitedKey &key) const noexcept {
    const auto elementHash = std::hash<const void *>{}(key.element);
    const auto typeHash = std::hash<std::type_index>{}(key.expectedType);
    return elementHash ^ (typeHash + 0x9e3779b9u + (elementHash << 6u) +
                          (elementHash >> 2u));
  }
};

struct DirectSubtypeEdge {
  std::type_index subtype = invalid_type();
  std::type_index supertype = invalid_type();

  [[nodiscard]] bool operator==(const DirectSubtypeEdge &) const noexcept =
      default;
};

struct DirectSubtypeEdgeHash {
  [[nodiscard]] std::size_t
  operator()(const DirectSubtypeEdge &edge) const noexcept {
    const auto subtypeHash = std::hash<std::type_index>{}(edge.subtype);
    const auto supertypeHash = std::hash<std::type_index>{}(edge.supertype);
    return subtypeHash ^ (supertypeHash + 0x9e3779b9u + (subtypeHash << 6u) +
                          (subtypeHash >> 2u));
  }
};

class AstReflectionBootstrapState {
public:
  explicit AstReflectionBootstrapState(AstReflection &reflection) noexcept
      : _reflection(std::addressof(reflection)) {}

  [[nodiscard]] bool beginVisit(const grammar::AbstractElement &element,
                                std::optional<std::type_index> expectedType) {
    return _visited
        .emplace(VisitedKey{.element = std::addressof(element),
                            .expectedType = expectedType.value_or(invalid_type())})
        .second;
  }

  void registerProducedType(const AstNodeTypeInfo &typeInfo,
                            std::optional<std::type_index> expectedType) {
    if (!is_valid_type(typeInfo.type)) {
      return;
    }

    registerKnownType(typeInfo.type);
    _producedTypesByType.try_emplace(typeInfo.type, std::addressof(typeInfo));

    if (expectedType.has_value() && is_valid_type(*expectedType) &&
        typeInfo.type != *expectedType) {
      addDirectSubtypeEdge(typeInfo.type, *expectedType);
    }
  }

  void registerAssignment(const AssignmentReflectionInfo &metadata) {
    if (metadata.assignedAstType != nullptr &&
        is_valid_type(metadata.assignedAstType->type)) {
      registerKnownType(metadata.assignedAstType->type);
    }

    if (metadata.referenceTargetType != nullptr &&
        is_valid_type(metadata.referenceTargetType->type)) {
      registerKnownType(metadata.referenceTargetType->type);
      _referenceAssignments.push_back(metadata.referenceTargetType);
    }
  }

  void finalize() {
    if (_finalized) {
      return;
    }
    _finalized = true;
    finalizeReferenceInducedEdges();
  }

private:
  void registerKnownType(std::type_index type) {
    if (!is_valid_type(type) || !_knownTypes.insert(type).second) {
      return;
    }
    _reflection->registerType(type);
  }

  void addDirectSubtypeEdge(std::type_index subtype,
                            std::type_index supertype) {
    if (!is_valid_type(subtype) || !is_valid_type(supertype) ||
        subtype == supertype) {
      registerKnownType(subtype);
      registerKnownType(supertype);
      return;
    }

    if (const DirectSubtypeEdge edge{.subtype = subtype, .supertype = supertype};
        !_directSubtypeEdges.insert(edge).second) {
      return;
    }

    registerKnownType(subtype);
    registerKnownType(supertype);
    _reflection->registerSubtype(subtype, supertype);
  }

  void finalizeReferenceInducedEdges() {
    for (const auto *targetTypeInfo : _referenceAssignments) {
      if (targetTypeInfo == nullptr ||
          !is_valid_type(targetTypeInfo->type) ||
          targetTypeInfo->isInstance == nullptr) {
        continue;
      }
      for (const auto &[producedType, typeInfo] : _producedTypesByType) {
        if (typeInfo == nullptr || producedType == targetTypeInfo->type ||
            typeInfo->probe == nullptr) {
          continue;
        }
        if (targetTypeInfo->isInstance(typeInfo->probe())) {
          addDirectSubtypeEdge(producedType, targetTypeInfo->type);
        }
      }
    }
  }

  AstReflection *_reflection = nullptr;
  std::unordered_set<VisitedKey, VisitedKeyHash> _visited;
  std::unordered_map<std::type_index, const AstNodeTypeInfo *>
      _producedTypesByType;
  std::vector<const AstNodeTypeInfo *> _referenceAssignments;
  std::unordered_set<std::type_index> _knownTypes;
  std::unordered_set<DirectSubtypeEdge, DirectSubtypeEdgeHash>
      _directSubtypeEdges;
  bool _finalized = false;
};

struct InitAccess {
  template <typename Expr>
    requires HasInitImpl<Expr>
  static void init(const Expr &expression, AstReflectionInitContext &ctx) {
    expression.init_impl(ctx);
  }

  template <typename Expr>
    requires(!HasInitImpl<Expr>)
  static void init(const Expr &, AstReflectionInitContext &) {}
};

} // namespace detail

struct AstReflectionInitContext {
  detail::AstReflectionBootstrapState *state = nullptr;
  std::optional<std::type_index> expectedType;

  [[nodiscard]] bool beginVisit(const grammar::AbstractElement &element) const {
    assert(state != nullptr);
    return state->beginVisit(element, expectedType);
  }

  void registerProducedType(const detail::AstNodeTypeInfo &typeInfo) const {
    assert(state != nullptr);
    state->registerProducedType(typeInfo, expectedType);
  }

  void registerAssignment(
      const detail::AssignmentReflectionInfo &metadata) const {
    assert(state != nullptr);
    state->registerAssignment(metadata);
  }

  [[nodiscard]] AstReflectionInitContext
  withExpectedType(std::type_index nextExpectedType) const noexcept {
    auto copy = *this;
    copy.expectedType = nextExpectedType;
    return copy;
  }

  [[nodiscard]] AstReflectionInitContext withoutExpectedType() const noexcept {
    auto copy = *this;
    copy.expectedType = std::nullopt;
    return copy;
  }
};

template <typename Expr>
  requires std::derived_from<std::remove_cvref_t<Expr>, grammar::AbstractElement>
inline void init(const Expr &expression, AstReflectionInitContext &ctx) {
  if (!ctx.beginVisit(expression)) {
    return;
  }
  detail::InitAccess::init(expression, ctx);
}

inline void bootstrapAstReflection(const grammar::ParserRule &entryRule,
                                   AstReflection &reflection) {
  detail::AstReflectionBootstrapState state{reflection};
  AstReflectionInitContext initContext{.state = std::addressof(state)};
  entryRule.init(initContext);
  state.finalize();
}

} // namespace pegium::parser
