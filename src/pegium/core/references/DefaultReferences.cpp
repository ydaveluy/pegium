#include <pegium/core/references/DefaultReferences.hpp>

#include <cassert>
#include <limits>
#include <optional>
#include <unordered_set>
#include <vector>

#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/syntax-tree/AstUtils.hpp>

namespace pegium::references {

namespace {

const AbstractReference *
find_reference_at_cst_node(const workspace::Document &document,
                           const CstNodeView &selectedNode) {
  const AbstractReference *best = nullptr;
  TextOffset bestSpan = std::numeric_limits<TextOffset>::max();
  for (const auto &handle : document.references) {
    const auto &reference = *handle.getConst();
    const auto refNode = reference.getRefNode();
    if (!refNode.has_value()) {
      continue;
    }
    if (selectedNode.getBegin() < refNode->getBegin() ||
        refNode->getEnd() < selectedNode.getEnd()) {
      continue;
    }
    const auto span = refNode->getEnd() - refNode->getBegin();
    if (best == nullptr || span < bestSpan) {
      best = &reference;
      bestSpan = span;
    }
  }
  return best;
}

void append_reference_targets(const AbstractReference &reference,
                              const pegium::CoreServices &services,
                              std::vector<const AstNode *> &results) {
  const auto &documents = *services.shared.workspace.documents;
  assert(reference.getContainer() != nullptr);
  const auto &currentDocument = getDocument(*reference.getContainer());
  std::unordered_set<workspace::NodeKey, workspace::NodeKeyHash> seen;
  const auto append =
      [&results, &seen, &documents,
       &currentDocument](const workspace::AstNodeDescription &description) {
        const auto key = workspace::NodeKey{.documentId = description.documentId,
                                            .symbolId = description.symbolId};
        if (!seen.insert(key).second) {
          return;
        }
        results.push_back(std::addressof(
            workspace::resolve_ast_node(documents, description,
                                        currentDocument)));
      };

  if (reference.isMultiReference()) {
    const auto &multi = static_cast<const AbstractMultiReference &>(reference);
    for (std::size_t index = 0; index < multi.resolvedDescriptionCount(); ++index) {
      append(multi.resolvedDescriptionAt(index));
    }
    return;
  }

  const auto &single = static_cast<const AbstractSingleReference &>(reference);
  if (single.isResolved()) {
    append(single.resolvedDescription());
  }
}

const AstNode *find_named_declaration(const AstNode &root,
                                      const CstNodeView &selectedNode,
                                      const pegium::CoreServices &services) {
  const auto *nameProvider = services.references.nameProvider.get();

  const AstNode *selected = nullptr;
  TextOffset selectedSpan = std::numeric_limits<TextOffset>::max();

  const auto consider = [&nameProvider, &selectedNode, &selected,
                         &selectedSpan](const AstNode &node) {
    const auto candidateNode = declaration_site_node(node, *nameProvider);
    if (!candidateNode.has_value()) {
      return;
    }
    if (selectedNode.getBegin() < candidateNode->getBegin() ||
        candidateNode->getEnd() < selectedNode.getEnd()) {
      return;
    }

    const auto span = candidateNode->getEnd() - candidateNode->getBegin();
    if (selected == nullptr || span < selectedSpan) {
      selected = &node;
      selectedSpan = span;
    }
  };

  consider(root);
  for (const auto *node : root.getAllContent()) {
    consider(*node);
  }

  return selected;
}

} // namespace

std::vector<const AstNode *>
DefaultReferences::findDeclarations(const CstNodeView &sourceCstNode) const {
  std::vector<const AstNode *> results;
  assert(sourceCstNode.valid());
  const auto &document = sourceCstNode.root().getDocument();

  if (const auto *reference =
          find_reference_at_cst_node(document, sourceCstNode);
      reference != nullptr) {
    append_reference_targets(*reference, services, results);
    return results;
  }

  if (document.hasAst()) {
    if (const auto *declaration = find_named_declaration(
            *document.parseResult.value, sourceCstNode, services);
        declaration != nullptr) {
      results.push_back(declaration);
    }
  }
  return results;
}

std::vector<CstNodeView>
DefaultReferences::findDeclarationNodes(const CstNodeView &sourceCstNode) const {
  std::vector<CstNodeView> results;
  const auto *nameProvider = services.references.nameProvider.get();
  for (const auto *node : findDeclarations(sourceCstNode)) {
    results.push_back(required_declaration_site_node(*node, *nameProvider));
  }
  return results;
}

std::vector<workspace::ReferenceDescription>
DefaultReferences::findReferences(const AstNode &targetNode,
                                  const FindReferencesOptions &options) const {
  const auto *indexManager = services.shared.workspace.indexManager.get();
  const auto *nameProvider = services.references.nameProvider.get();
  const auto &document = getDocument(targetNode);

  std::vector<workspace::ReferenceDescription> results;
  if (options.includeDeclaration) {
    const auto declarationNode =
        required_declaration_site_node(targetNode, *nameProvider);
    results.push_back(workspace::ReferenceDescription{
        .sourceDocumentId = document.id,
        .sourceOffset = declarationNode.getBegin(),
        .sourceLength = declarationNode.getEnd() - declarationNode.getBegin(),
        .referenceType = std::type_index(typeid(void)),
        .local = true,
        .targetDocumentId = document.id,
        .targetSymbolId = document.makeSymbolId(targetNode),
    });
  }

  const auto targetKey = workspace::NodeKey{
      .documentId = document.id,
      .symbolId = document.makeSymbolId(targetNode),
  };
  for (const auto &reference : indexManager->findAllReferences(targetKey)) {
    if (options.documentId.has_value() &&
        reference.sourceDocumentId != *options.documentId) {
      continue;
    }
    results.push_back(reference);
  }

  return results;
}

} // namespace pegium::references
