#include <pegium/converters/AstJsonConverter.hpp>

#include <cassert>
#include <limits>
#include <stdexcept>
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
#include <pegium/core/workspace/DefaultAstNodeLocator.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::converter {
namespace {

using FeatureAssignments = std::vector<const grammar::Assignment *>;

const AstNode *root_node(const AstNode &node) {
  auto *current = &node;
  while (current->getContainer() != nullptr) {
    current = current->getContainer();
  }
  return current;
}

bool belongs_to_root(const AstNode &node, const AstNode &root) {
  return root_node(node) == &root;
}

const workspace::AstNodeLocator &ast_node_locator() {
  static const workspace::DefaultAstNodeLocator locator;
  return locator;
}

std::string build_ast_path(const AstNode &node, const AstNode &root) {
  if (!belongs_to_root(node, root)) {
    return {};
  }

  try {
    return "#" + ast_node_locator().getAstNodePath(node);
  } catch (const std::invalid_argument &) {
    return {};
  }
}

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
convert_feature_value(const grammar::FeatureValue &value, const AstNode &root,
                      const AstJsonConversionOptions &options);

pegium::JsonValue convert_reference(const AbstractReference &reference,
                                      const AstNode &root,
                                      const AstJsonConversionOptions &options) {
  pegium::JsonValue::Object object;
  const auto *document = tryGetDocument(root);
  const bool canResolve =
      document == nullptr ||
      document->state >= workspace::DocumentState::ComputedScopes;

  if (options.includeReferenceText && !reference.getRefText().empty()) {
    object.try_emplace("$refText", reference.getRefText());
  }

  pegium::JsonValue::Array refs;
  if (reference.isMultiReference()) {
    const auto *multi = static_cast<const AbstractMultiReference *>(&reference);
    if (reference.isResolved() || canResolve) {
      const auto count = multi->resolvedDescriptionCount();
      refs.reserve(count);
      for (std::size_t index = 0; index < count; ++index) {
        const auto &description = multi->resolvedDescriptionAt(index);
        if (document == nullptr || description.documentId != document->id) {
          continue;
        }
        const auto &target = document->getAstNode(description.symbolId);
        const auto path = build_ast_path(target, root);
        if (!path.empty()) {
          refs.emplace_back(path);
        }
      }
    }
    object.try_emplace("$refs", std::move(refs));
  } else if (reference.isResolved() || canResolve) {
    if (const auto *target =
            static_cast<const AbstractSingleReference &>(reference).resolve();
        target != nullptr) {
      const auto path = build_ast_path(*target, root);
      if (!path.empty()) {
        object.try_emplace("$ref", std::move(path));
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
convert_feature_value(const grammar::FeatureValue &value, const AstNode &root,
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
    return reference == nullptr ? pegium::JsonValue(nullptr)
                                : convert_reference(*reference, root, options);
  }

  pegium::JsonValue::Array array;
  for (const auto &item : value.array()) {
    array.emplace_back(convert_feature_value(item, root, options));
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
  const auto *root = root_node(node);
  for (const auto *assignment : assignments) {
    const auto feature = std::string(assignment->getFeature());
    if (!seenFeatures.insert(feature).second) {
      continue;
    }
    object.try_emplace(
        feature,
        convert_feature_value(assignment->getValue(&node), *root, options));
  }

  return pegium::JsonValue(std::move(object));
}

} // namespace pegium::converter
