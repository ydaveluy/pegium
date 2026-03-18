#include <pegium/references/DefaultReferences.hpp>

#include <algorithm>
#include <cctype>
#include <limits>
#include <ranges>
#include <string>

#include <pegium/services/CoreServices.hpp>
#include <pegium/services/SharedCoreServices.hpp>

namespace pegium::references {

namespace {

bool is_word_char(char c) noexcept {
  const unsigned char uc = static_cast<unsigned char>(c);
  return std::isalnum(uc) != 0 || c == '_';
}

std::string token_at(std::string_view text, TextOffset offset) {
  const auto size = static_cast<TextOffset>(text.size());
  TextOffset cursor = std::min(offset, size);

  if (cursor == size && cursor > 0 && is_word_char(text[cursor - 1])) {
    --cursor;
  } else if (cursor < size && !is_word_char(text[cursor]) && cursor > 0 &&
             is_word_char(text[cursor - 1])) {
    --cursor;
  }
  if (cursor >= size || !is_word_char(text[cursor])) {
    return {};
  }

  TextOffset begin = cursor;
  TextOffset end = cursor + 1;
  while (begin > 0 && is_word_char(text[begin - 1])) {
    --begin;
  }
  while (end < size && is_word_char(text[end])) {
    ++end;
  }

  return std::string(text.substr(begin, end - begin));
}

const AbstractReference *find_reference_at_offset(
    const workspace::Document &document, TextOffset offset) {
  const AbstractReference *best = nullptr;
  TextOffset bestSpan = std::numeric_limits<TextOffset>::max();
  for (const auto &handle : document.references) {
    const auto *reference = handle.getConst();
    if (reference == nullptr) {
      continue;
    }
    const auto refNode = reference->getRefNode();
    if (!refNode.has_value()) {
      continue;
    }
    if (offset < refNode->getBegin() || offset > refNode->getEnd()) {
      continue;
    }
    const auto span = refNode->getEnd() - refNode->getBegin();
    if (best == nullptr || span < bestSpan) {
      best = reference;
      bestSpan = span;
    }
  }
  return best;
}

std::optional<workspace::NodeKey>
reference_target_key(const workspace::Document &document, TextOffset offset) {
  const auto *reference = find_reference_at_offset(document, offset);
  if (reference == nullptr) {
    return std::nullopt;
  }

  (void)reference->resolve();
  if (reference->getTargetDocumentId() == workspace::InvalidDocumentId) {
    return std::nullopt;
  }

  for (const auto &description : document.referenceDescriptions) {
    if (description.sourceOffset != reference->getRefNode()->getBegin()) {
      continue;
    }
    const auto targetKey = description.targetKey();
    if (targetKey.has_value()) {
      return targetKey;
    }
  }

  return std::nullopt;
}

std::optional<workspace::AstNodeDescription>
declaration_at_named_node_offset(const workspace::Document &document,
                                 TextOffset offset,
                                 const services::CoreServices &coreServices) {
  const auto *nameProvider = coreServices.references.nameProvider.get();
  const auto *descriptionProvider =
      coreServices.workspace.astNodeDescriptionProvider.get();
  const auto *root = document.parseResult.value.get();
  if (nameProvider == nullptr || descriptionProvider == nullptr || root == nullptr) {
    return std::nullopt;
  }

  const AstNode *selected = nullptr;
  TextOffset selectedSpan = std::numeric_limits<TextOffset>::max();

  const auto consider =
      [&nameProvider, offset, &selected, &selectedSpan](const AstNode &node) {
    const auto nameNode = nameProvider->getNameNode(node);
    if (!nameNode.valid()) {
      return;
    }
    if (offset < nameNode.getBegin() || offset > nameNode.getEnd()) {
      return;
    }

    const auto span = nameNode.getEnd() - nameNode.getBegin();
    if (selected == nullptr || span < selectedSpan) {
      selected = &node;
      selectedSpan = span;
    }
      };

  consider(*root);
  for (const auto *node : root->getAllContent()) {
    if (node != nullptr) {
      consider(*node);
    }
  }

  if (selected == nullptr) {
    return std::nullopt;
  }
  return descriptionProvider->createDescription(*selected, document);
}

} // namespace

std::optional<workspace::AstNodeDescription> DefaultReferences::findDeclarationAt(
    const workspace::Document &document, TextOffset offset) const {
  const auto *indexManager = coreServices.shared.workspace.indexManager.get();
  if (indexManager == nullptr) {
    return std::nullopt;
  }

  if (const auto targetKey = reference_target_key(document, offset);
      targetKey.has_value()) {
    for (const auto &entry : indexManager->findAllReferences(*targetKey, true)) {
      if (std::holds_alternative<workspace::AstNodeDescription>(entry)) {
        return std::get<workspace::AstNodeDescription>(entry);
      }
    }
  }

  if (auto declaration =
          declaration_at_named_node_offset(document, offset, coreServices);
      declaration.has_value()) {
    return declaration;
  }

  const auto token = token_at(document.text(), offset);
  if (token.empty()) {
    return std::nullopt;
  }
  for (const auto &entry : indexManager->findElementsByName(token)) {
    return entry;
  }
  return std::nullopt;
}

utils::stream<workspace::ReferenceDescriptionOrDeclaration>
DefaultReferences::findReferencesAt(const workspace::Document &document,
                                    TextOffset offset,
                                    bool includeDeclaration) const {
  const auto *indexManager = coreServices.shared.workspace.indexManager.get();
  if (indexManager == nullptr) {
    return utils::make_stream<workspace::ReferenceDescriptionOrDeclaration>(
        std::views::empty<workspace::ReferenceDescriptionOrDeclaration>);
  }

  if (const auto targetKey = reference_target_key(document, offset);
      targetKey.has_value()) {
    return indexManager->findAllReferences(*targetKey, includeDeclaration);
  }

  const auto declaration = findDeclarationAt(document, offset);
  if (!declaration.has_value()) {
    return utils::make_stream<workspace::ReferenceDescriptionOrDeclaration>(
        std::views::empty<workspace::ReferenceDescriptionOrDeclaration>);
  }

  return indexManager->findAllReferences(
      workspace::NodeKey{.documentId = declaration->documentId,
                         .symbolId = declaration->symbolId},
      includeDeclaration);
}

} // namespace pegium::references
