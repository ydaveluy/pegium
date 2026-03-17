#include <pegium/documentation/DefaultDocumentationProvider.hpp>

#include <cctype>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include <pegium/services/CoreServices.hpp>
#include <pegium/services/SharedCoreServices.hpp>
#include <pegium/syntax-tree/AstUtils.hpp>
#include <pegium/utils/Stream.hpp>
#include <pegium/workspace/Document.hpp>
#include <pegium/workspace/Documents.hpp>
#include <pegium/workspace/IndexManager.hpp>

namespace pegium::documentation {

namespace {

std::string trim(std::string_view text) {
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
  return std::string(text.substr(begin, end - begin));
}

bool is_jsdoc_comment(std::string_view comment) {
  const auto cleaned = trim(comment);
  return cleaned.size() >= 5 && cleaned.starts_with("/**") &&
         cleaned.ends_with("*/");
}

std::string strip_jsdoc_delimiters(std::string_view comment) {
  auto cleaned = trim(comment);
  if (cleaned.starts_with("/**")) {
    cleaned.erase(0, 3);
  }
  if (cleaned.ends_with("*/")) {
    cleaned.erase(cleaned.size() - 2);
  }
  return trim(cleaned);
}

std::string normalize_jsdoc_line(std::string_view line) {
  auto trimmed = trim(line);
  if (!trimmed.empty() && trimmed.front() == '*') {
    trimmed.erase(trimmed.begin());
    if (!trimmed.empty() &&
        std::isspace(static_cast<unsigned char>(trimmed.front())) != 0) {
      trimmed.erase(trimmed.begin());
    }
  }
  return trimmed;
}

std::vector<std::string> split_jsdoc_lines(std::string_view comment) {
  const auto body = strip_jsdoc_delimiters(comment);
  std::vector<std::string> lines;
  std::size_t begin = 0;
  while (begin <= body.size()) {
    const auto end = body.find('\n', begin);
    const auto slice =
        end == std::string::npos
            ? std::string_view(body).substr(begin)
            : std::string_view(body).substr(begin, end - begin);
    auto normalized = normalize_jsdoc_line(slice);
    lines.push_back(std::move(normalized));
    if (end == std::string::npos) {
      break;
    }
    begin = end + 1;
  }
  return lines;
}

std::string render_jsdoc_markdown(std::vector<std::string> lines) {
  std::string markdown;
  for (const auto &line : lines) {
    if (!markdown.empty()) {
      markdown.push_back('\n');
    }
    markdown += line;
  }
  return trim(markdown);
}

} // namespace

std::optional<std::string> DefaultDocumentationProvider::getDocumentation(
    const AstNode &node) const {
  if (coreServices.documentation.commentProvider == nullptr) {
    return std::nullopt;
  }

  const auto commentView =
      coreServices.documentation.commentProvider->getComment(node);
  if (commentView.empty()) {
    return std::nullopt;
  }

  const auto cleanedComment = trim(commentView);
  if (cleanedComment.empty()) {
    return std::nullopt;
  }

  if (!is_jsdoc_comment(cleanedComment)) {
    return std::nullopt;
  }

  auto lines = split_jsdoc_lines(cleanedComment);
  for (auto &line : lines) {
    line = normalizeJsdocLinks(node, std::move(line));
    if (!line.empty() && line.front() == '@') {
      line = documentationTagRenderer(node, line).value_or("- `" + line + "`");
    }
  }
  return render_jsdoc_markdown(std::move(lines));
}

std::optional<std::string>
DefaultDocumentationProvider::documentationLinkRenderer(
    const AstNode &node, std::string_view name, std::string_view display) const {
  auto description = findNameInLocalSymbols(node, name);
  if (!description.has_value()) {
    description = findNameInGlobalScope(node, name);
  }
  if (!description.has_value() || description->nameLength == 0) {
    return std::nullopt;
  }

  const auto *contextDocument = tryGetDocument(node);
  std::shared_ptr<workspace::Document> targetDocument;
  if (contextDocument != nullptr &&
      description->documentId == contextDocument->id) {
    targetDocument = coreServices.shared.workspace.documents != nullptr
                         ? coreServices.shared.workspace.documents->getDocument(
                               contextDocument->id)
                         : nullptr;
  } else if (coreServices.shared.workspace.documents != nullptr) {
    targetDocument = coreServices.shared.workspace.documents->getDocument(
        description->documentId);
  }

  if (targetDocument == nullptr) {
    return std::nullopt;
  }

  const auto position = targetDocument->offsetToPosition(description->offset);
  const auto uri = std::format("{}#L{},{}", targetDocument->uri,
                               position.line + 1, position.character + 1);
  return std::format("[{}]({})", display, uri);
}

std::optional<std::string>
DefaultDocumentationProvider::documentationTagRenderer(
    const AstNode &node, std::string_view tag) const {
  (void)node;
  return std::format("- `{}`", tag);
}

std::string
DefaultDocumentationProvider::normalizeJsdocLinks(const AstNode &node,
                                                  std::string line) const {
  const std::string_view open = "{@link";
  std::size_t cursor = 0;
  while ((cursor = line.find(open, cursor)) != std::string::npos) {
    const auto end = line.find('}', cursor);
    if (end == std::string::npos) {
      break;
    }

    const auto payload = trim(std::string_view(line).substr(
        cursor + open.size(), end - (cursor + open.size())));
    if (payload.empty()) {
      line.erase(cursor, end - cursor + 1);
      continue;
    }

    std::string target;
    std::string display;
    if (const auto pipe = payload.find('|'); pipe != std::string::npos) {
      target = trim(payload.substr(0, pipe));
      display = trim(payload.substr(pipe + 1));
    } else if (const auto separator = payload.find_first_of(" \t");
               separator != std::string::npos) {
      target = trim(payload.substr(0, separator));
      display = trim(payload.substr(separator + 1));
    } else {
      target = trim(payload);
    }

    if (display.empty()) {
      display = target;
    }

    const auto replacement =
        documentationLinkRenderer(node, target, display).value_or(display);
    line.replace(cursor, end - cursor + 1, replacement);
    cursor += replacement.size();
  }
  return line;
}

std::optional<workspace::AstNodeDescription>
DefaultDocumentationProvider::findNameInLocalSymbols(const AstNode &node,
                                                     std::string_view name) const {
  const auto *document = tryGetDocument(node);
  if (document == nullptr || document->localSymbols.empty()) {
    return std::nullopt;
  }

  for (auto current = &node; current != nullptr; current = current->getContainer()) {
    const auto [begin, end] = document->localSymbols.equal_range(current);
    for (auto it = begin; it != end; ++it) {
      if (it->second.name == name) {
        return it->second;
      }
    }
  }

  return std::nullopt;
}

std::optional<workspace::AstNodeDescription>
DefaultDocumentationProvider::findNameInGlobalScope(
    const AstNode &node, std::string_view name) const {
  (void)node;
  const auto *indexManager = coreServices.shared.workspace.indexManager.get();
  if (indexManager == nullptr) {
    return std::nullopt;
  }

  auto matches = utils::collect(indexManager->findElementsByName(name));
  if (matches.empty()) {
    return std::nullopt;
  }
  return matches.front();
}
} // namespace pegium::documentation
