#include <pegium/converter/AstJsonConverter.hpp>

#include <limits>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include <pegium/grammar/Assignment.hpp>
#include <pegium/grammar/FeatureValue.hpp>
#include <pegium/parser/Introspection.hpp>
#include <pegium/syntax-tree/Reference.hpp>
#include <pegium/utils/TransparentStringHash.hpp>

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

std::string build_ast_path(const AstNode &node, const AstNode &root) {
  if (!belongs_to_root(node, root)) {
    return {};
  }

  std::vector<std::string> segments;
  for (auto *current = &node; current != &root; current = current->getContainer()) {
    if (current == nullptr || current->getContainer() == nullptr) {
      return {};
    }

    std::string segment = "/";
    if (current->getContainerPropertyName().empty()) {
      return {};
    }

    segment += current->getContainerPropertyName();
    if (const auto index = current->getContainerPropertyIndex();
        index.has_value()) {
      segment += "@";
      segment += std::to_string(*index);
    }
    segments.push_back(std::move(segment));
  }

  std::string path = "#";
  for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
    path += *it;
  }
  return path;
}

void collect_child_ast_root_ids(const AstNode &node,
                                std::unordered_set<NodeId> &ids) {
  for (const auto *child : node.getContent()) {
    if (child != nullptr && child->hasCstNode()) {
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
    collect_feature_assignments(child, childRootIds, visitedNodeIds, assignments);
  }
}

services::JsonValue convert_rule_value(const grammar::RuleValue &value) {
  return std::visit(
      []<typename T>(const T &item) {
        using Value = std::remove_cvref_t<T>;
        if constexpr (std::same_as<Value, std::nullptr_t>) {
          return services::JsonValue(nullptr);
        } else if constexpr (std::same_as<Value, bool>) {
          return services::JsonValue(item);
        } else if constexpr (std::same_as<Value, char>) {
          return services::JsonValue(std::string(1, item));
        } else if constexpr (std::same_as<Value, std::string_view>) {
          return services::JsonValue(std::string(item));
        } else if constexpr (std::same_as<Value, std::string>) {
          return services::JsonValue(item);
        } else if constexpr (std::floating_point<Value>) {
          return services::JsonValue(static_cast<double>(item));
        } else if constexpr (std::unsigned_integral<Value>) {
          if (item >
              static_cast<Value>(std::numeric_limits<std::int64_t>::max())) {
            return services::JsonValue(std::to_string(item));
          }
          return services::JsonValue(static_cast<std::int64_t>(item));
        } else if constexpr (std::signed_integral<Value>) {
          return services::JsonValue(static_cast<std::int64_t>(item));
        } else {
          return services::JsonValue(nullptr);
        }
      },
      value);
}

services::JsonValue convert_feature_value(const grammar::FeatureValue &value,
                                          const AstNode &root,
                                          const AstJsonConversionOptions &options);

services::JsonValue convert_reference(const AbstractReference &reference,
                                      const AstNode &root,
                                      const AstJsonConversionOptions &options) {
  services::JsonValue::Object object;

  if (options.includeReferenceText && !reference.getRefText().empty()) {
    object.try_emplace("$refText", reference.getRefText());
  }

  const auto resolved = reference.resolveAll();
  services::JsonValue::Array refs;
  refs.reserve(resolved.size());
  for (const auto *target : resolved) {
    if (target == nullptr) {
      continue;
    }
    const auto path = build_ast_path(*target, root);
    if (!path.empty()) {
      refs.emplace_back(path);
    }
  }

  if (reference.isMulti()) {
    object.try_emplace("$refs", std::move(refs));
  } else if (!refs.empty()) {
    object.try_emplace("$ref", std::move(refs.front()));
  }

  if (options.includeReferenceErrors && reference.hasError() &&
      !reference.getErrorMessage().empty()) {
    object.try_emplace("$error", reference.getErrorMessage());
  }

  return services::JsonValue(std::move(object));
}

services::JsonValue convert_feature_value(const grammar::FeatureValue &value,
                                          const AstNode &root,
                                          const AstJsonConversionOptions &options) {
  if (value.isRuleValue()) {
    return convert_rule_value(value.ruleValue());
  }
  if (value.isAstNode()) {
    const auto *node = value.astNode();
    return node == nullptr ? services::JsonValue(nullptr)
                           : AstJsonConverter::convert(*node, options);
  }
  if (value.isReference()) {
    const auto *reference = value.reference().value;
    return reference == nullptr ? services::JsonValue(nullptr)
                                : convert_reference(*reference, root, options);
  }

  services::JsonValue::Array array;
  for (const auto &item : value.array()) {
    array.emplace_back(convert_feature_value(item, root, options));
  }
  return services::JsonValue(std::move(array));
}

} // namespace

services::JsonValue AstJsonConverter::convert(const AstNode &node,
                                              const Options &options) {
  services::JsonValue::Object object;
  if (options.includeType) {
    object.try_emplace("$type", parser::detail::runtime_type_name(typeid(node)));
  }

  if (!node.hasCstNode()) {
    return services::JsonValue(std::move(object));
  }

  std::unordered_set<NodeId> childRootIds;
  collect_child_ast_root_ids(node, childRootIds);

  FeatureAssignments assignments;
  std::unordered_set<NodeId> visitedNodeIds;
  for (const auto &child : node.getCstNode()) {
    collect_feature_assignments(child, childRootIds, visitedNodeIds, assignments);
  }

  utils::TransparentStringSet seenFeatures;
  const auto *root = root_node(node);
  for (const auto *assignment : assignments) {
    const auto feature = std::string(assignment->getFeature());
    if (!seenFeatures.insert(feature).second) {
      continue;
    }
    object.try_emplace(feature, convert_feature_value(assignment->getValue(&node),
                                                      *root, options));
  }

  return services::JsonValue(std::move(object));
}

} // namespace pegium::converter
