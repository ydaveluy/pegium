#pragma once

#include <pegium/grammar/Assignment.hpp>
#include <pegium/grammar/Create.hpp>
#include <pegium/grammar/InfixRule.hpp>
#include <pegium/grammar/Nest.hpp>
#include <pegium/grammar/ParserRule.hpp>
#include <pegium/parser/ValueBuildContext.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <array>
#include <cassert>
#include <memory>
#include <vector>

namespace pegium::parser::detail {

template <typename T> struct ParserRuleBuildSupport {
  [[nodiscard]] static std::unique_ptr<T>
  get_raw_value(const CstNodeView &node, const ValueBuildContext &context) {
    auto buildContext = context;
    std::vector<ReferenceHandle> localReferences;
    if (buildContext.references == nullptr) {
      buildContext.references = &localReferences;
    }
    std::unique_ptr<AstNode> currentNode;
    struct PendingAssignmentEntry {
      const grammar::Assignment *assignment;
      NodeId nodeId;
    };
    std::array<PendingAssignmentEntry, 8> pendingInlineEntries{};
    std::size_t pendingInlineCount = 0;
    std::vector<PendingAssignmentEntry> pendingOverflowEntries;
    const auto &root = node.root();

    const auto clearPendingAssignments = [&]() noexcept {
      pendingInlineCount = 0;
      pendingOverflowEntries.clear();
    };

    const auto addPendingAssignment = [&](const grammar::Assignment *assignment,
                                          NodeId nodeId) {
      if (pendingInlineCount < pendingInlineEntries.size()) {
        pendingInlineEntries[pendingInlineCount++] = PendingAssignmentEntry{
            .assignment = assignment,
            .nodeId = nodeId,
        };
        return;
      }
      pendingOverflowEntries.push_back(PendingAssignmentEntry{
          .assignment = assignment,
          .nodeId = nodeId,
      });
    };

    const auto applyPendingAssignments = [&](AstNode *targetNode) {
      if (pendingInlineCount == 0 && pendingOverflowEntries.empty()) [[likely]] {
        return;
      }
      assert(targetNode != nullptr);
      if (pendingInlineCount == 1 && pendingOverflowEntries.empty()) [[likely]] {
        const auto &entry = pendingInlineEntries[0];
        entry.assignment->execute(targetNode, CstNodeView(&root, entry.nodeId),
                                  buildContext);
        return;
      }
      for (std::size_t index = 0; index < pendingInlineCount; ++index) {
        const auto &entry = pendingInlineEntries[index];
        entry.assignment->execute(targetNode, CstNodeView(&root, entry.nodeId),
                                  buildContext);
      }
      for (const auto &entry : pendingOverflowEntries) {
        entry.assignment->execute(targetNode, CstNodeView(&root, entry.nodeId),
                                  buildContext);
      }
    };

    const auto applyAndClearPendingAssignments = [&](AstNode *targetNode) {
      applyPendingAssignments(targetNode);
      clearPendingAssignments();
    };

    if (node.isLeaf()) {
      auto leafNode = std::make_unique<T>();
      leafNode->setCstNode(node);
      applyPendingAssignments(leafNode.get());
      return leafNode;
    }

    for (const auto view : node) {
      const auto &cstNode = view.node();
      if (cstNode.isHidden) {
        continue;
      }
      switch (view.getGrammarElement()->getKind()) {
      case grammar::ElementKind::Assignment: {
        addPendingAssignment(
            static_cast<const grammar::Assignment *>(cstNode.grammarElement),
            view.id());
        break;
      }
      case grammar::ElementKind::Create: {
        currentNode =
            static_cast<const grammar::Create *>(cstNode.grammarElement)
                ->getValue();
        assert(currentNode != nullptr);
        currentNode->setCstNode(node);
        break;
      }
      case grammar::ElementKind::Nest: {
        if (!currentNode) {
          currentNode = std::make_unique<T>();
          currentNode->setCstNode(node);
        }
        applyAndClearPendingAssignments(currentNode.get());
        currentNode =
            static_cast<const grammar::Nest *>(cstNode.grammarElement)
                ->getValue(std::move(currentNode));
        assert(currentNode != nullptr);
        currentNode->setCstNode(node);
        break;
      }
      case grammar::ElementKind::ParserRule: {
        currentNode =
            static_cast<const grammar::ParserRule *>(cstNode.grammarElement)
                ->getValue(view, buildContext);
        assert(currentNode != nullptr && currentNode->hasCstNode());
        break;
      }
      case grammar::ElementKind::InfixRule: {
        const auto *infixRule =
            static_cast<const grammar::InfixRule *>(cstNode.grammarElement);
        applyAndClearPendingAssignments(currentNode.get());
        currentNode =
            infixRule->getValue(view, std::move(currentNode), buildContext);
        assert(currentNode != nullptr && currentNode->hasCstNode());
        break;
      }
      default:
        break;
      }
    }

    if (!currentNode) {
      currentNode = std::make_unique<T>();
      currentNode->setCstNode(node);
    }
    applyPendingAssignments(currentNode.get());
    auto *typedNode = static_cast<T *>(currentNode.get());
    assert(dynamic_cast<T *>(currentNode.get()));
    currentNode.release();
    return std::unique_ptr<T>(typedNode);
  }
};

} // namespace pegium::parser::detail
