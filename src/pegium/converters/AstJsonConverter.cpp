#include <pegium/converters/AstJsonConverter.hpp>

#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/grammar/FeatureValue.hpp>
#include <pegium/core/parser/Introspection.hpp>
#include <pegium/core/syntax-tree/AstUtils.hpp>
#include <pegium/core/syntax-tree/Reference.hpp>
#include <pegium/core/utils/TransparentStringHash.hpp>
#include <pegium/core/workspace/Document.hpp>
#include <pegium/core/workspace/Symbol.hpp>

namespace pegium {
namespace {

using FeatureAssignments = std::vector<const grammar::Assignment *>;

void collect_child_ast_root_ids(const AstNode &node,
                                std::unordered_set<NodeId> &ids) {
  for (const auto *child : node.getContent()) {
    if (child->hasCstNode()) {
      ids.insert(child->getCstNode().id());
    }
  }
}

void collect_feature_assignments(const CstNodeView &node,
                                 const std::unordered_set<NodeId> &childRootIds,
                                 std::unordered_set<NodeId> &visitedNodeIds,
                                 FeatureAssignments &assignments) {
  if (!node.valid()) {
    return;
  }
  if (node.isHidden()) {
    return;
  }
  if (!visitedNodeIds.insert(node.id()).second) {
    return;
  }
  if (node.getGrammarElement()->getKind() == grammar::ElementKind::Assignment) {
    assignments.push_back(
        static_cast<const grammar::Assignment *>(node.getGrammarElement()));
    return;
  }

  if (childRootIds.contains(node.id())) {
    return;
  }

  for (const auto &child : node) {
    collect_feature_assignments(child, childRootIds, visitedNodeIds,
                                assignments);
  }
}

// Builds the JSON `$ref` value pointing to a node within the same document.
// Cross-document references are intentionally skipped.
[[nodiscard]] std::optional<std::string>
build_local_ref(const workspace::AstNodeDescription &description,
                const workspace::Document *document) {
  if (document == nullptr || description.documentId != document->id ||
      description.symbolId == workspace::InvalidSymbolId) {
    return std::nullopt;
  }
  return "#" + std::to_string(description.symbolId);
}

pegium::JsonValue convert_rule_value(const grammar::RuleValue &value) {
  return std::visit(
      []<typename T>(const T &item) {
        using Value = std::remove_cvref_t<T>;
        if constexpr (std::same_as<Value, std::nullptr_t>) {
          return pegium::JsonValue(nullptr);
        } else if constexpr (std::same_as<Value, bool>) {
          return pegium::JsonValue(item);
        } else if constexpr (std::same_as<Value, char>) {
          return pegium::JsonValue(std::string(1, item));
        } else if constexpr (std::same_as<Value, std::string_view>) {
          return pegium::JsonValue(std::string(item));
        } else if constexpr (std::same_as<Value, std::string>) {
          return pegium::JsonValue(item);
        } else if constexpr (std::floating_point<Value>) {
          return pegium::JsonValue(static_cast<double>(item));
        } else if constexpr (std::unsigned_integral<Value>) {
          if (item >
              static_cast<Value>(std::numeric_limits<std::int64_t>::max())) {
            return pegium::JsonValue(std::to_string(item));
          }
          return pegium::JsonValue(static_cast<std::int64_t>(item));
        } else if constexpr (std::signed_integral<Value>) {
          return pegium::JsonValue(static_cast<std::int64_t>(item));
        } else {
          return pegium::JsonValue(nullptr);
        }
      },
      value);
}

pegium::JsonValue
convert_feature_value(const grammar::FeatureValue &value,
                      const workspace::Document *document,
                      const AstJsonConversionOptions &options);

pegium::JsonValue convert_reference(const AbstractReference &reference,
                                    const workspace::Document *document,
                                    const AstJsonConversionOptions &options) {
  pegium::JsonValue::Object object;
  const bool canResolve =
      document == nullptr ||
      document->state >= workspace::DocumentState::ComputedScopes;

  if (options.includeReferenceText && !reference.getRefText().empty()) {
    object.try_emplace("$refText", reference.getRefText());
  }

  if (reference.isMultiReference()) {
    pegium::JsonValue::Array refs;
    const auto *multi = static_cast<const AbstractMultiReference *>(&reference);
    if (reference.isResolved() || canResolve) {
      const auto count = multi->resolvedDescriptionCount();
      refs.reserve(count);
      for (std::size_t index = 0; index < count; ++index) {
        const auto &description = multi->resolvedDescriptionAt(index);
        if (auto ref = build_local_ref(description, document); ref) {
          refs.emplace_back(std::move(*ref));
        }
      }
    }
    object.try_emplace("$refs", std::move(refs));
  } else if (reference.isResolved() || canResolve) {
    const auto &single =
        static_cast<const AbstractSingleReference &>(reference);
    if (single.resolve() != nullptr) {
      if (auto ref = build_local_ref(single.resolvedDescription(), document);
          ref) {
        object.try_emplace("$ref", std::move(*ref));
      }
    }
  }

  if (options.includeReferenceErrors && reference.hasError() &&
      !reference.getErrorMessage().empty()) {
    object.try_emplace("$error", std::string(reference.getErrorMessage()));
  }

  return pegium::JsonValue(std::move(object));
}

pegium::JsonValue
convert_feature_value(const grammar::FeatureValue &value,
                      const workspace::Document *document,
                      const AstJsonConversionOptions &options) {
  if (value.isRuleValue()) {
    return convert_rule_value(value.ruleValue());
  }
  if (value.isAstNode()) {
    const auto *node = value.astNode();
    return node == nullptr ? pegium::JsonValue(nullptr)
                           : AstJsonConverter::convert(*node, options);
  }
  if (value.isReference()) {
    const auto *reference = value.reference().value;
    return reference == nullptr
               ? pegium::JsonValue(nullptr)
               : convert_reference(*reference, document, options);
  }

  pegium::JsonValue::Array array;
  for (const auto &item : value.array()) {
    array.emplace_back(convert_feature_value(item, document, options));
  }
  return pegium::JsonValue(std::move(array));
}

} // namespace

pegium::JsonValue AstJsonConverter::convert(const AstNode &node,
                                              const Options &options) {
  pegium::JsonValue::Object object;
  if (options.includeType) {
    object.try_emplace("$type",
                       parser::detail::runtime_type_name(typeid(node)));
  }

  if (!node.hasCstNode()) {
    return pegium::JsonValue(std::move(object));
  }

  std::unordered_set<NodeId> childRootIds;
  collect_child_ast_root_ids(node, childRootIds);

  FeatureAssignments assignments;
  std::unordered_set<NodeId> visitedNodeIds;
  for (const auto &child : node.getCstNode()) {
    collect_feature_assignments(child, childRootIds, visitedNodeIds,
                                assignments);
  }

  utils::TransparentStringSet seenFeatures;
  const auto *document = tryGetDocument(node);
  for (const auto *assignment : assignments) {
    const auto feature = std::string(assignment->getFeature());
    if (!seenFeatures.insert(feature).second) {
      continue;
    }
    object.try_emplace(
        feature,
        convert_feature_value(assignment->getValue(&node), document, options));
  }

  return pegium::JsonValue(std::move(object));
}

} // namespace pegium
