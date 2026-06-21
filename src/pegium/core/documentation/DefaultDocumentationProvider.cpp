#include <pegium/core/documentation/DefaultDocumentationProvider.hpp>

#include <cassert>
#include <cctype>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include <pegium/core/documentation/DocComment.hpp>
#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/syntax-tree/AstUtils.hpp>
#include <pegium/core/workspace/Document.hpp>
#include <pegium/core/workspace/Documents.hpp>
#include <pegium/core/workspace/IndexManager.hpp>

namespace pegium::documentation {

namespace {

std::string_view trim(std::string_view text) {
  std::size_t begin = 0;
  std::size_t end = text.size();
  while (begin < end &&
         std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
    ++begin;
  }
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return text.substr(begin, end - begin);
}

bool is_link_tag(std::string_view name) {
  return name == "link" || name == "linkplain" || name == "linkcode";
}

// Splits an inline link's content into a `target` and a `display` value. The
// two may be separated by `|` or whitespace; with no separator the target is
// also used as the display.
std::pair<std::string, std::string> split_link_content(std::string_view content) {
  auto separator = content.find('|');
  if (separator == std::string_view::npos) {
    separator = content.find_first_of(" \t");
  }
  if (separator == std::string_view::npos) {
    const auto target = std::string(trim(content));
    return {target, target};
  }
  auto target = std::string(trim(content.substr(0, separator)));
  auto display = std::string(trim(content.substr(separator + 1)));
  if (display.empty()) {
    display = target;
  }
  return {std::move(target), std::move(display)};
}

} // namespace

std::optional<std::string> DefaultDocumentationProvider::getDocumentation(
    const AstNode &node) const {
  const auto comment =
      services.documentation.commentProvider->getComment(node);
  // is_doc_comment already rejects empty / whitespace-only input (its prefix
  // marker fails on the empty front line), so no separate trim().empty() check
  // is needed.
  if (!is_doc_comment(comment)) {
    return std::nullopt;
  }

  const auto parsed = parse_doc_comment(comment);

  DocCommentRenderOptions options;
  options.renderTag =
      [this, &node](std::string_view name, std::string_view content,
                    bool inlineTag) -> std::optional<std::string> {
    // Inline links resolve their target to the declaration's source location;
    // everything else defers to the user-overridable tag hook.
    if (inlineTag && is_link_tag(name)) {
      const auto [target, display] = split_link_content(content);
      if (auto resolved = documentationLinkRenderer(node, target, display);
          resolved.has_value()) {
        return resolved;
      }
      return display;
    }
    return documentationTagRenderer(node, name, content, inlineTag);
  };

  auto markdown = parsed.toMarkdown(options);
  if (markdown.empty()) {
    return std::nullopt;
  }
  return markdown;
}

std::optional<std::string>
DefaultDocumentationProvider::documentationLinkRenderer(
    const AstNode &node, std::string_view name, std::string_view display) const {
  auto description = findNameInLocalSymbols(node, name);
  if (!description.has_value()) {
    description = findNameInGlobalScope(node, name);
  }
  if (!description.has_value()) {
    return std::nullopt;
  }
  assert(description->nameLength > 0);

  const auto &contextDocument = getDocument(node);
  const workspace::Document *targetDocument = &contextDocument;
  std::shared_ptr<workspace::Document> targetHolder;
  if (description->documentId != contextDocument.id) {
    targetHolder = services.shared.workspace.documents->getDocument(
        description->documentId);
    if (targetHolder == nullptr) {
      // The target lives in a document that resolved through the global index
      // but is no longer loaded (e.g. evicted or deleted during a reindex).
      // Degrade to plain display text rather than dereferencing null.
      return std::nullopt;
    }
    targetDocument = targetHolder.get();
  }

  const auto position =
      targetDocument->textDocument().positionAt(description->offset);
  const auto uri = std::format("{}#L{},{}", targetDocument->uri,
                               position.line + 1, position.character + 1);
  return std::format("[{}]({})", display, uri);
}

std::optional<std::string>
DefaultDocumentationProvider::documentationTagRenderer(
    const AstNode &node, std::string_view name, std::string_view content,
    bool inlineTag) const {
  (void)node;
  (void)name;
  (void)content;
  (void)inlineTag;
  return std::nullopt;
}

std::optional<workspace::AstNodeDescription>
DefaultDocumentationProvider::findNameInLocalSymbols(const AstNode &node,
                                                     std::string_view name) const {
  const auto &document = getDocument(node);
  if (document.localSymbols.empty()) {
    return std::nullopt;
  }

  for (const auto *current = &node; current != nullptr;
       current = current->getContainer()) {
    const auto *entries = document.localSymbols.forContainer(current);
    if (entries == nullptr) {
      continue;
    }
    for (const auto &bucket : *entries) {
      const auto it = bucket.entriesByName.find(name);
      if (it != bucket.entriesByName.end() && !it->second.empty()) {
        return *it->second.first;
      }
    }
  }

  return std::nullopt;
}

std::optional<workspace::AstNodeDescription>
DefaultDocumentationProvider::findNameInGlobalScope(
    const AstNode &node, std::string_view name) const {
  (void)node;
  const auto *indexManager = services.shared.workspace.indexManager.get();
  for (const auto &entry : indexManager->allElements()) {
    if (entry.name == name) {
      return entry;
    }
  }
  return std::nullopt;
}

} // namespace pegium::documentation
