#include <pegium/lsp/DefaultCompletionProvider.hpp>

#include <algorithm>
#include <cctype>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <pegium/documentation/DocumentationProvider.hpp>
#include <pegium/grammar/AbstractRule.hpp>
#include <pegium/grammar/Literal.hpp>
#include <pegium/lsp/LspProviderUtils.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/services/SharedServices.hpp>
#include <pegium/syntax-tree/AstUtils.hpp>
#include <pegium/utils/Stream.hpp>
#include <pegium/utils/TransparentStringHash.hpp>

namespace pegium::lsp {

using namespace detail;

namespace {

std::string description_detail(const workspace::AstNodeDescription &description) {
  return detail::display_type_name(description.type);
}

const AbstractReference *
find_reference_at_offset(const workspace::Document &document, TextOffset offset) {
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

std::string_view completion_prefix(std::string_view text, const TokenSpan &token,
                                   TextOffset offset) {
  if (token.text.empty()) {
    return {};
  }
  const auto safeOffset = std::clamp(offset, token.begin, token.end);
  return text.substr(token.begin, safeOffset - token.begin);
}

std::optional<::lsp::MarkupContent>
reference_documentation(const workspace::AstNodeDescription &description,
                        const documentation::DocumentationProvider *provider) {
  if (provider == nullptr || description.node == nullptr) {
    return std::nullopt;
  }

  const auto documentation = provider->getDocumentation(*description.node);
  if (!documentation.has_value() || documentation->empty()) {
    return std::nullopt;
  }

  ::lsp::MarkupContent markup{};
  markup.kind = ::lsp::MarkupKind::Markdown;
  markup.value = *documentation;
  return markup;
}

std::string completion_key(std::string_view label, std::string_view detail) {
  std::string key;
  key.reserve(label.size() + detail.size() + 1);
  key.append(label);
  key.push_back('\x1f');
  key.append(detail);
  return key;
}

const AstNode *find_completion_node(const workspace::Document &document,
                                    TextOffset anchorOffset) {
  if (document.parseResult.value == nullptr) {
    return nullptr;
  }

  const auto *root = document.parseResult.value.get();
  if (const auto *node = find_ast_node_at_offset(*root, anchorOffset);
      node != nullptr) {
    return node;
  }
  if (anchorOffset > 0) {
    if (const auto *node = find_ast_node_at_offset(*root, anchorOffset - 1);
        node != nullptr) {
      return node;
    }
  }

  if (const auto size = static_cast<TextOffset>(document.textView().size());
      anchorOffset < size) {
    if (const auto *node = find_ast_node_at_offset(*root, anchorOffset + 1);
        node != nullptr) {
      return node;
    }
  }

  return root;
}

bool reference_matches_feature(const AbstractReference &reference,
                               const parser::ExpectPath &feature) {
  const auto *assignment = feature.expectedReferenceAssignment();
  if (assignment == nullptr) {
    return false;
  }
  if (reference.getReferenceType() != assignment->getReferenceType()) {
    return false;
  }
  const auto featureName = assignment->getFeature();
  return featureName.empty() || reference.getProperty() == featureName;
}

std::optional<::lsp::TextEdit>
build_completion_text_edit(const CompletionContext &context,
                           std::string_view matchText, std::string_view newText,
                           const FuzzyMatcher &matcher) {
  if (!matcher.match(context.prefix, matchText)) {
    return std::nullopt;
  }

  ::lsp::TextEdit textEdit{};
  textEdit.newText = std::string(newText);
  textEdit.range.start = context.document.offsetToPosition(context.tokenOffset);
  textEdit.range.end = context.document.offsetToPosition(context.offset);
  return textEdit;
}

std::string default_sort_text(const CompletionContext &context,
                              const CompletionValue &value) {
  if (value.insertTextFormat.has_value() &&
      *value.insertTextFormat == ::lsp::InsertTextFormat::Snippet) {
    return "1";
  }
  if (context.feature == nullptr) {
    return "9";
  }
  if (context.feature->expectsReference()) {
    return "0";
  }
  if (context.feature->expectsRule()) {
    return "1";
  }
  if (context.feature->expectsKeyword()) {
    return "2";
  }
  return "9";
}

} // namespace

std::optional<::lsp::CompletionList>
DefaultCompletionProvider::getCompletion(
    const workspace::Document &document, const ::lsp::CompletionParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);

  if (const auto *fuzzyMatcher =
          languageServices.sharedServices.lsp.fuzzyMatcher.get();
      fuzzyMatcher == nullptr) {
    return std::nullopt;
  }

  const auto offset = document.positionToOffset(params.position);
  const auto token = token_at(document.textView(), offset);
  const auto tokenOffset = token.text.empty() ? offset : token.begin;
  const auto tokenEndOffset = token.text.empty() ? offset : token.end;
  const auto prefix = completion_prefix(document.textView(), token, offset);
  const auto *node = find_completion_node(document, tokenOffset);
  const auto *reference = find_reference_at_offset(document, offset);

  std::vector<::lsp::CompletionItem> items;
  utils::TransparentStringMap<std::size_t> itemsByKey;

  const auto accept =
      [this, &items, &itemsByKey](const CompletionContext &context,
                                  const CompletionValue &value) {
    ::lsp::CompletionItem item{};
    if (!fillCompletionItem(context, value, item)) {
      return;
    }

    const auto detail = item.detail.has_value()
                            ? std::string_view(*item.detail)
                            : std::string_view{};
    const auto [it, inserted] =
        itemsByKey.try_emplace(completion_key(item.label, detail), items.size());
    if (!inserted) {
      return;
    }
    items.push_back(std::move(item));
  };

  const auto expect =
      languageServices.parser == nullptr
          ? parser::ExpectResult{}
          : languageServices.parser->expect(document.textView(), tokenOffset,
                                            cancelToken);
  const auto &alternatives = expect.frontier;

  for (const auto &feature : alternatives) {
    utils::throw_if_cancelled(cancelToken);
    CompletionContext context{document, params, offset, tokenOffset,
                              tokenEndOffset, token.text, prefix, node,
                              reference, &feature};
    completionFor(context, [&accept, &context](CompletionValue value) {
      accept(context, std::move(value));
    });
    if (!continueCompletion(context)) {
      break;
    }
  }

  if (items.empty() && offset > tokenOffset) {
    const auto suffixExpect =
        languageServices.parser == nullptr
            ? parser::ExpectResult{}
            : languageServices.parser->expect(document.textView(), offset,
                                              cancelToken);
    const auto &suffixAlternatives = suffixExpect.frontier;
    if (!suffixAlternatives.empty()) {
      for (const auto &feature : suffixAlternatives) {
        utils::throw_if_cancelled(cancelToken);
        CompletionContext context{document, params, offset, offset, offset,
                                  {},          {},     node,   reference,
                                  &feature};
        completionFor(context, [&accept, &context](CompletionValue value) {
          accept(context, std::move(value));
        });
        if (!continueCompletion(context)) {
          break;
        }
      }
    }
  }

  ::lsp::CompletionList completionList{};
  completionList.isIncomplete = true;
  completionList.items.reserve(items.size());
  for (auto &item : items) {
    utils::throw_if_cancelled(cancelToken);
    completionList.items.push_back(std::move(item));
  }
  return std::optional<::lsp::CompletionList>{std::move(completionList)};
}

void DefaultCompletionProvider::completionFor(
    const CompletionContext &context,
    const CompletionAcceptor &acceptor) const {
  if (context.feature == nullptr || !context.feature->isValid()) {
    return;
  }

  if (context.feature->expectsReference()) {
    completionForReference(context, makeScopeQueryContext(context), acceptor);
    return;
  }
  if (const auto *expectedRule = context.feature->expectedRule();
      expectedRule != nullptr) {
    completionForRule(context, *expectedRule, acceptor);
    return;
  }
  if (const auto *contextRule = context.feature->contextRule();
      contextRule != nullptr) {
    completionForRule(context, *contextRule, acceptor);
  }
  if (const auto *literal = context.feature->literal(); literal != nullptr) {
    completionForKeyword(context, *literal, acceptor);
  }
}

void DefaultCompletionProvider::completionForReference(
    const CompletionContext &context,
    const references::ScopeQueryContext &scopeContext,
    const CompletionAcceptor &acceptor) const {
  for (const auto *candidate : getReferenceCandidates(context, scopeContext)) {
    if (candidate == nullptr) {
      continue;
    }
    auto value =
        createReferenceCompletionItem(context, scopeContext, *candidate);
    if (!value.has_value()) {
      continue;
    }
    acceptor(std::move(*value));
  }
}

std::vector<const workspace::AstNodeDescription *>
DefaultCompletionProvider::getReferenceCandidates(
    const CompletionContext &context,
    const references::ScopeQueryContext &scopeContext) const {
  (void)context;
  const auto *scopeProvider = languageServices.references.scopeProvider.get();
  if (scopeProvider == nullptr) {
    return {};
  }
  auto query = scopeContext;
  query.referenceText = {};
  return utils::collect(scopeProvider->getScopeEntries(query));
}

std::optional<CompletionValue>
DefaultCompletionProvider::createReferenceCompletionItem(
    const CompletionContext &context,
    const references::ScopeQueryContext &scopeContext,
    const workspace::AstNodeDescription &candidate) const {
  (void)context;
  (void)scopeContext;
  if (candidate.name.empty()) {
    return std::nullopt;
  }

  CompletionValue value;
  value.label = candidate.name;
  value.description = &candidate;
  value.sortText = "0";
  return value;
}

void DefaultCompletionProvider::completionForRule(
    const CompletionContext &context, const grammar::AbstractRule &rule,
    const CompletionAcceptor &acceptor) const {
  (void)context;
  if (rule.getName().empty()) {
    return;
  }

  CompletionValue value;
  value.label = rule.getName();
  value.newText = std::string("${1:") + std::string(rule.getName()) + "}";
  value.detail = "Rule";
  value.kind = ::lsp::CompletionItemKind::Snippet;
  value.insertTextFormat = ::lsp::InsertTextFormat::Snippet;
  acceptor(std::move(value));
}

void DefaultCompletionProvider::completionForKeyword(
    const CompletionContext &context, const grammar::Literal &keyword,
    const CompletionAcceptor &acceptor) const {
  if (!filterKeyword(context, keyword)) {
    return;
  }

  CompletionValue value;
  value.label = std::string(keyword.getValue());
  value.detail = "Keyword";
  value.kind = ::lsp::CompletionItemKind::Keyword;
  value.sortText = "2";
  acceptor(std::move(value));
}

bool DefaultCompletionProvider::filterKeyword(
    const CompletionContext &context, const grammar::Literal &keyword) const {
  (void)context;
  return !keyword.getValue().empty();
}

bool DefaultCompletionProvider::fillCompletionItem(
    const CompletionContext &context, const CompletionValue &value,
    ::lsp::CompletionItem &item) const {
  const auto *fuzzyMatcher =
      languageServices.sharedServices.lsp.fuzzyMatcher.get();
  if (fuzzyMatcher == nullptr || value.label.empty()) {
    return false;
  }

  const auto matchText =
      value.filterText.has_value() ? std::string_view(*value.filterText)
                                   : std::string_view(value.label);
  const auto insertText =
      value.newText.empty() ? std::string_view(value.label)
                            : std::string_view(value.newText);
  const auto textEdit = value.textEdit.has_value()
                            ? value.textEdit
                            : build_completion_text_edit(context, matchText,
                                                         insertText,
                                                         *fuzzyMatcher);
  if (!textEdit.has_value()) {
    return false;
  }

  const auto *nodeKindProvider =
      languageServices.sharedServices.lsp.nodeKindProvider.get();
  const auto *documentationProvider =
      languageServices.documentation.documentationProvider.get();

  item.label = value.label;
  item.textEdit = *textEdit;

  if (value.insertTextFormat.has_value()) {
    item.insertTextFormat = *value.insertTextFormat;
  }

  if (value.kind.has_value()) {
    item.kind = *value.kind;
  } else if (value.insertTextFormat.has_value() &&
             *value.insertTextFormat == ::lsp::InsertTextFormat::Snippet) {
    item.kind = ::lsp::CompletionItemKind::Snippet;
  } else if (value.description != nullptr && nodeKindProvider != nullptr) {
    item.kind = nodeKindProvider->getCompletionItemKind(*value.description);
  } else if (context.feature != nullptr &&
             context.feature->expectsKeyword()) {
    item.kind = ::lsp::CompletionItemKind::Keyword;
  } else if (context.feature != nullptr &&
             context.feature->expectsReference()) {
    item.kind = ::lsp::CompletionItemKind::Reference;
  }

  if (value.detail.has_value()) {
    item.detail = *value.detail;
  } else if (value.description != nullptr) {
    item.detail = description_detail(*value.description);
  } else if (context.feature != nullptr &&
             context.feature->expectsKeyword()) {
    item.detail = "Keyword";
  }

  if (value.sortText.has_value()) {
    item.sortText = *value.sortText;
  } else {
    item.sortText = default_sort_text(context, value);
  }

  if (value.filterText.has_value()) {
    item.filterText = *value.filterText;
  }

  if (value.documentation.has_value()) {
    item.documentation = *value.documentation;
  } else if (value.description != nullptr) {
    if (auto documentation =
            reference_documentation(*value.description, documentationProvider);
        documentation.has_value()) {
      item.documentation = *documentation;
    }
  }

  return true;
}

bool DefaultCompletionProvider::continueCompletion(
    const CompletionContext &context) const {
  (void)context;
  return true;
}

references::ScopeQueryContext
DefaultCompletionProvider::makeScopeQueryContext(
    const CompletionContext &context) const {
  if (context.feature == nullptr) {
    return {};
  }

  const auto *assignment = context.feature->expectedReferenceAssignment();
  if (assignment == nullptr) {
    return {};
  }
  if (context.reference != nullptr &&
      reference_matches_feature(*context.reference, *context.feature)) {
    auto query = references::makeScopeQueryContext(
        makeReferenceInfo(*context.reference));
    query.referenceText = context.prefix;
    query.rule = context.feature->contextRule();
    query.assignment = assignment;
    query.multi = context.reference->isMulti();
    return query;
  }

  auto *container = const_cast<AstNode *>(context.node);
  if (container == nullptr && context.document.parseResult.value != nullptr) {
    container = context.document.parseResult.value.get();
  }

  return references::ScopeQueryContext{
      .reference = nullptr,
      .container = container,
      .property = assignment->getFeature(),
      .index = std::nullopt,
      .referenceText = context.prefix,
      .referenceType = assignment->getReferenceType(),
      .rule = context.feature->contextRule(),
      .assignment = assignment,
      .multi = assignment->isMultiReference(),
  };
}

} // namespace pegium::lsp
