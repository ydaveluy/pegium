#include <pegium/lsp/LspProviderUtils.hpp>

#include <algorithm>
#include <cctype>
#include <format>
#include <limits>

#include <pegium/grammar/AbstractRule.hpp>
#include <pegium/grammar/Assignment.hpp>
#include <pegium/grammar/Create.hpp>
#include <pegium/grammar/Literal.hpp>
#include <pegium/grammar/Nest.hpp>
#include <pegium/parser/Introspection.hpp>
#include <pegium/services/SharedServices.hpp>
#include <pegium/references/NameProvider.hpp>
#include <pegium/utils/UriUtils.hpp>

namespace pegium::lsp::detail {

namespace {

bool is_word_char(char c) noexcept {
  const unsigned char uc = static_cast<unsigned char>(c);
  return std::isalnum(uc) != 0 || c == '_';
}

::lsp::Range offset_to_range(const workspace::TextDocument &document,
                             TextOffset begin, TextOffset end) {
  ::lsp::Range range{};
  range.start = document.offsetToPosition(begin);
  range.end = document.offsetToPosition(end >= begin ? end : begin);
  return range;
}

bool has_newline_between(std::string_view text, TextOffset begin,
                         TextOffset end) {
  const auto textSize = static_cast<TextOffset>(text.size());
  const auto safeBegin = std::ranges::min(begin, textSize);
  const auto safeEnd = std::ranges::min(end, textSize);
  if (safeEnd <= safeBegin) {
    return false;
  }
  return text.substr(safeBegin, safeEnd - safeBegin).find('\n') !=
         std::string_view::npos;
}

struct SelectionRangeSegment {
  TextOffset begin = 0;
  TextOffset end = 0;
};

void append_if_distinct(std::vector<SelectionRangeSegment> &chain,
                        SelectionRangeSegment range) {
  if (!chain.empty() && chain.back().begin == range.begin &&
      chain.back().end == range.end) {
    return;
  }
  chain.push_back(std::move(range));
}

bool collect_node_chain_for_offset(const CstNodeView &node, TextOffset offset,
                                   std::vector<SelectionRangeSegment> &chain) {
  if (node.isHidden()) {
    return false;
  }
  if (offset < node.getBegin() || offset > node.getEnd()) {
    return false;
  }

  for (const auto &child : node) {
    if (collect_node_chain_for_offset(child, offset, chain)) {
      append_if_distinct(chain, {.begin = node.getBegin(), .end = node.getEnd()});
      return true;
    }
  }

  append_if_distinct(chain, {.begin = node.getBegin(), .end = node.getEnd()});
  return true;
}

} // namespace

TokenSpan token_at(std::string_view text, TextOffset offset) {
  const auto size = static_cast<TextOffset>(text.size());
  TextOffset cursor = std::ranges::min(offset, size);

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

  return {begin, end, text.substr(begin, end - begin)};
}

std::string grammar_label(const grammar::AbstractElement *element) {
  if (element == nullptr) {
    return {};
  }
  using enum grammar::ElementKind;
  switch (element->getKind()) {
  case ParserRule:
  case DataTypeRule:
  case TerminalRule:
  case InfixRule: {
    const auto *rule = static_cast<const grammar::AbstractRule *>(element);
    if (const auto name = rule->getName(); !name.empty()) {
      return std::string(name);
    }
    return std::string(rule->getTypeName());
  }
  case Literal:
    return std::string(
        static_cast<const grammar::Literal *>(element)->getValue());
  case Assignment: {
    const auto *assignment = static_cast<const grammar::Assignment *>(element);
    return "assign " + std::string(assignment->getFeature());
  }
  case Create:
    return "create " +
           std::string(static_cast<const grammar::Create *>(element)
                           ->getTypeName());
  case Nest:
    return "nest " + std::string(
                        static_cast<const grammar::Nest *>(element)
                            ->getFeature());
  case Group:
    return "group";
  case OrderedChoice:
    return "ordered-choice";
  case UnorderedGroup:
    return "unordered-group";
  case Repetition:
    return "repetition";
  case CharacterRange:
    return "character-range";
  case AnyCharacter:
    return "any-character";
  case AndPredicate:
    return "and-predicate";
  case NotPredicate:
    return "not-predicate";
  case InfixOperator:
    return "infix-operator";
  }
  return "grammar-element";
}

std::string display_type_name(std::type_index type) {
  if (type == std::type_index(typeid(void))) {
    return {};
  }
  return parser::detail::runtime_type_name(type);
}

std::string location_key(const LocationData &location) {
  return std::format("{}#{}:{}", location.documentId, location.begin,
                     location.end);
}

LocationData to_location(const workspace::AstNodeDescription &symbol) {
  const auto nameLength = symbol.nameLength;
  return {.documentId = symbol.documentId,
          .begin = symbol.offset,
          .end = symbol.offset + nameLength};
}

LocationData to_location(const workspace::ReferenceDescription &reference) {
  return {.documentId = reference.sourceDocumentId,
          .begin = reference.sourceOffset,
          .end = reference.sourceOffset + reference.sourceLength};
}

std::optional<LocationData> resolve_reference_target_location(
    const workspace::Document &document, TextOffset offset,
    const services::Services &services) {
  const auto *nameProvider = services.references.nameProvider.get();
  if (nameProvider == nullptr) {
    return std::nullopt;
  }

  const AbstractReference *selectedReference = nullptr;
  TextOffset selectedWidth = std::numeric_limits<TextOffset>::max();

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

    const auto width = refNode->getEnd() - refNode->getBegin();
    if (selectedReference == nullptr || width < selectedWidth) {
      selectedReference = reference;
      selectedWidth = width;
    }
  }

  if (selectedReference == nullptr) {
    return std::nullopt;
  }

  AstNode *targetNode = selectedReference->resolve();
  if (targetNode == nullptr) {
    return std::nullopt;
  }

  const auto targetDocumentId = selectedReference->getTargetDocumentId();
  if (targetDocumentId == workspace::InvalidDocumentId) {
    return std::nullopt;
  }

  if (const auto nameNode = nameProvider->getNameNode(*targetNode); nameNode) {
    return LocationData{
        .documentId = targetDocumentId,
        .begin = nameNode.getBegin(),
        .end = nameNode.getEnd(),
    };
  }

  if (targetNode->hasCstNode()) {
    const auto &cstNode = targetNode->getCstNode();
    return LocationData{
        .documentId = targetDocumentId,
        .begin = cstNode.getBegin(),
        .end = cstNode.getEnd(),
    };
  }

  return std::nullopt;
}

std::shared_ptr<const workspace::TextDocument>
resolve_text_document(workspace::DocumentId documentId,
                      const services::SharedServices &sharedServices) {
  if (sharedServices.workspace.documents != nullptr) {
    if (auto document = sharedServices.workspace.documents->getDocument(documentId);
        document != nullptr && document->textDocument() != nullptr) {
      return document->textDocument();
    }
  }
  return nullptr;
}

std::optional<::lsp::LocationLink>
to_location_link(const workspace::Document &sourceDocument,
                 TextOffset sourceOffset,
                 const LocationData &targetLocation,
                 const services::SharedServices &sharedServices) {
  const auto sourceTextDocument = sourceDocument.textDocument();
  if (sourceTextDocument == nullptr) {
    return std::nullopt;
  }

  const auto targetDocument =
      resolve_text_document(targetLocation.documentId, sharedServices);
  if (targetDocument == nullptr) {
    return std::nullopt;
  }
  if (sharedServices.workspace.documents == nullptr) {
    return std::nullopt;
  }
  const auto targetWorkspaceDocument =
      sharedServices.workspace.documents->getDocument(targetLocation.documentId);
  if (targetWorkspaceDocument == nullptr) {
    return std::nullopt;
  }

  const auto sourceToken = token_at(sourceDocument.text(), sourceOffset);
  const auto originSelectionRange =
      sourceToken.text.empty()
          ? offset_to_range(*sourceTextDocument, sourceOffset, sourceOffset)
          : offset_to_range(*sourceTextDocument, sourceToken.begin,
                            sourceToken.end);
  const auto targetRange =
      offset_to_range(*targetDocument, targetLocation.begin, targetLocation.end);

  ::lsp::LocationLink link{};
  link.targetUri = ::lsp::Uri::parse(targetWorkspaceDocument->uri);
  link.targetRange = targetRange;
  link.targetSelectionRange = targetRange;
  link.originSelectionRange = originSelectionRange;
  return link;
}

std::optional<::lsp::WorkspaceEdit>
to_lsp_workspace_edit(const WorkspaceEditData &workspaceEdit,
                      const services::SharedServices &sharedServices,
                      const utils::CancellationToken &cancelToken) {
  ::lsp::WorkspaceEdit lspWorkspaceEdit{};
  ::lsp::Map<::lsp::DocumentUri, ::lsp::Array<::lsp::TextEdit>> changes;

  if (sharedServices.workspace.documents == nullptr) {
    return std::nullopt;
  }

  for (const auto &[documentId, edits] : workspaceEdit.changes) {
    utils::throw_if_cancelled(cancelToken);
    const auto document = sharedServices.workspace.documents->getDocument(documentId);
    if (document == nullptr || document->textDocument() == nullptr) {
      continue;
    }
    const auto textDocument = document->textDocument();

    auto &lspEdits = changes[::lsp::Uri::parse(document->uri)];
    lspEdits.reserve(edits.size());
    for (const auto &edit : edits) {
      utils::throw_if_cancelled(cancelToken);
      ::lsp::TextEdit lspEdit{};
      lspEdit.range = offset_to_range(*textDocument, edit.begin, edit.end);
      lspEdit.newText = edit.newText;
      lspEdits.push_back(std::move(lspEdit));
    }
  }

  if (changes.empty()) {
    return std::nullopt;
  }

  lspWorkspaceEdit.changes = std::move(changes);
  return lspWorkspaceEdit;
}

std::string document_highlight_key(const DocumentHighlightData &highlight) {
  return std::format("{}:{}", highlight.begin, highlight.end);
}

void collect_folding_ranges(const CstNodeView &node, std::string_view text,
                            std::vector<FoldingRangeData> &ranges,
                            utils::TransparentStringSet &seen) {
  if (node.isHidden()) {
    return;
  }

  if (node.getEnd() > node.getBegin() &&
      has_newline_between(text, node.getBegin(), node.getEnd())) {
    FoldingRangeData range{
        .begin = node.getBegin(),
        .end = node.getEnd(),
        .kind = ::lsp::FoldingRangeKind::Region,
    };
    const auto key = std::format("{}:{}", range.begin, range.end);
    if (seen.insert(key).second) {
      ranges.push_back(std::move(range));
    }
  }

  for (const auto &child : node) {
    collect_folding_ranges(child, text, ranges, seen);
  }
}

bool is_link_end_char(char c) noexcept {
  switch (c) {
  case ' ':
  case '\t':
  case '\r':
  case '\n':
  case '"':
  case '\'':
  case '<':
  case '>':
  case '(':
  case ')':
  case '[':
  case ']':
  case '{':
  case '}':
    return true;
  default:
    return false;
  }
}

::lsp::SelectionRange compute_selection_range(const workspace::Document &document,
                                              TextOffset offset) {
  std::vector<SelectionRangeSegment> chain;
  if (const auto token = token_at(document.text(), offset); !token.text.empty()) {
    append_if_distinct(chain, {.begin = token.begin, .end = token.end});
  }

  if (document.parseResult.cst != nullptr) {
    for (const auto &child : *document.parseResult.cst) {
      if (collect_node_chain_for_offset(child, offset, chain)) {
        break;
      }
    }
  }

  ::lsp::SelectionRange selection{};
  if (chain.empty()) {
    ::lsp::Range range{};
    range.start = document.offsetToPosition(offset);
    range.end = document.offsetToPosition(offset);
    selection.range = range;
    return selection;
  }

  ::lsp::Range firstRange{};
  firstRange.start = document.offsetToPosition(chain.front().begin);
  firstRange.end = document.offsetToPosition(chain.front().end);
  selection.range = firstRange;
  auto *current = &selection;
  for (std::size_t index = 1; index < chain.size(); ++index) {
    current->parent = std::make_unique<::lsp::SelectionRange>();
    current = current->parent.get();
    ::lsp::Range range{};
    range.start = document.offsetToPosition(chain[index].begin);
    range.end = document.offsetToPosition(chain[index].end);
    current->range = range;
  }
  return selection;
}

bool has_symbol_or_reference(std::string_view token,
                             const workspace::IndexManager &index,
                             const workspace::Documents *documents) {
  if (auto symbols = index.findElementsByName(token);
      symbols.begin() != symbols.end()) {
    return true;
  }

  if (documents == nullptr) {
    return false;
  }

  for (const auto &document : documents->all()) {
    if (document == nullptr) {
      continue;
    }
    for (const auto &ref : index.referenceDescriptionsForDocument(document->id)) {
      if (ref.sourceText(document->textView()) == token) {
        return true;
      }
    }
  }
  return false;
}

} // namespace pegium::lsp::detail
