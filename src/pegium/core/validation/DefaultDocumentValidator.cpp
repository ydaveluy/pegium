#include <pegium/core/validation/DefaultDocumentValidator.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <pegium/core/grammar/AbstractRule.hpp>
#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/grammar/Literal.hpp>
#include <pegium/core/parser/ContextShared.hpp>
#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/syntax-tree/CstNodeView.hpp>
#include <pegium/core/syntax-tree/CstUtils.hpp>
#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>

namespace pegium::validation {

namespace {

constexpr std::string_view kDefaultCodeActionsKey = "pegiumDefaultCodeActions";

struct FoundToken {
  TextOffset begin = 0;
  TextOffset end = 0;
  std::string_view image;
};

[[nodiscard]] std::optional<CstNodeView>
next_visible_leaf_at_or_after(const workspace::Document &document,
                              TextOffset offset) {
  if (document.parseResult.cst == nullptr) {
    return std::nullopt;
  }
  for (auto leaf = find_first_leaf(*document.parseResult.cst); leaf.has_value();
       leaf = find_next_leaf(*leaf)) {
    if (leaf->isHidden() || leaf->getEnd() <= offset) {
      continue;
    }
    return leaf;
  }
  return std::nullopt;
}

[[nodiscard]] FoundToken find_found_token(const workspace::Document &document,
                                          TextOffset offset);

ValidationAcceptor
make_collecting_acceptor(std::vector<pegium::Diagnostic> &diagnostics,
                         const std::string &source) {
  return ValidationAcceptor{
      [&diagnostics, &source](pegium::Diagnostic diagnostic) {
    if (diagnostic.source.empty()) {
      diagnostic.source = source;
    }
    diagnostics.push_back(std::move(diagnostic));
  }};
}

[[nodiscard]] std::string quote_keyword(std::string_view value) {
  return "'" + std::string(value) + "'";
}

[[nodiscard]] std::string quote_list_item(std::string_view value) {
  return value.size() >= 2 && value.front() == '\'' && value.back() == '\''
             ? std::string(value)
             : quote_keyword(value);
}

[[nodiscard]] const grammar::Literal *
literal_expectation(const grammar::AbstractElement *element) noexcept {
  if (element == nullptr) {
    return nullptr;
  }
  if (element->getKind() == grammar::ElementKind::Assignment) {
    return literal_expectation(
        static_cast<const grammar::Assignment *>(element)->getElement());
  }
  return element->getKind() == grammar::ElementKind::Literal
             ? static_cast<const grammar::Literal *>(element)
             : nullptr;
}

void append_default_code_action_data(pegium::Diagnostic &diagnostic,
                                     std::string editKind, std::string title,
                                     TextOffset begin, TextOffset end,
                                     std::string newText) {
  pegium::JsonValue::Object action;
  action.try_emplace("kind", "quickfix");
  action.try_emplace("editKind", std::move(editKind));
  action.try_emplace("title", std::move(title));
  action.try_emplace("begin", static_cast<std::int64_t>(begin));
  action.try_emplace("end", static_cast<std::int64_t>(end));
  action.try_emplace("newText", std::move(newText));

  pegium::JsonValue::Array actions;
  actions.emplace_back(std::move(action));

  pegium::JsonValue::Object data;
  data.try_emplace(std::string(kDefaultCodeActionsKey), std::move(actions));
  diagnostic.data = pegium::JsonValue(std::move(data));
}

[[nodiscard]] std::string
format_expect_element(const grammar::AbstractElement *element) {
  if (element == nullptr) {
    return {};
  }

  using enum grammar::ElementKind;
  switch (element->getKind()) {
  case Literal: {
    const auto *literal = static_cast<const grammar::Literal *>(element);
    const auto value = literal->getValue();
    if (value.empty()) {
      return {};
    }
    return value.size() == 1u ? std::string(value) : quote_keyword(value);
  }
  case Assignment:
    return format_expect_element(
        static_cast<const grammar::Assignment *>(element)->getElement());
  case DataTypeRule:
  case ParserRule:
  case TerminalRule:
  case InfixRule:
    return std::string(
        static_cast<const grammar::AbstractRule *>(element)->getName());
  default:
    return {};
  }
}

void maybe_attach_insert_code_action(pegium::Diagnostic &diagnostic,
                                     const parser::ParseDiagnostic &parseDiagnostic,
                                     TextOffset offset) {
  const auto *literal = literal_expectation(parseDiagnostic.element);
  if (literal == nullptr || literal->getValue().empty()) {
    return;
  }
  append_default_code_action_data(
      diagnostic, "insert",
      "Insert " + format_expect_element(literal), offset, offset,
      std::string(literal->getValue()));
}

void maybe_attach_replace_code_action(pegium::Diagnostic &diagnostic,
                                      const parser::ParseDiagnostic &parseDiagnostic,
                                      TextOffset begin, TextOffset end) {
  const auto *literal = literal_expectation(parseDiagnostic.element);
  if (literal == nullptr || literal->getValue().empty() || end <= begin) {
    return;
  }
  append_default_code_action_data(
      diagnostic, "replace",
      "Replace with " + format_expect_element(literal), begin, end,
      std::string(literal->getValue()));
}

void maybe_attach_delete_code_action(pegium::Diagnostic &diagnostic) {
  if (diagnostic.end <= diagnostic.begin) {
    return;
  }
  append_default_code_action_data(diagnostic, "delete", "Delete unexpected text",
                                  diagnostic.begin, diagnostic.end, "");
}

[[nodiscard]] std::string format_expect_path(const parser::ExpectPath &path) {
  if (!path.isValid()) {
    return {};
  }

  if (const auto *literal = path.literal(); literal != nullptr) {
    return format_expect_element(literal);
  }
  if (const auto *assignment = path.expectedReferenceAssignment();
      assignment != nullptr) {
    return format_expect_element(assignment->getElement());
  }
  if (const auto *rule = path.expectedRule(); rule != nullptr) {
    return format_expect_element(rule);
  }
  return format_expect_element(path.expectedElement());
}

[[nodiscard]] std::string
format_expect_alternatives(std::span<const std::string> alternatives) {
  if (alternatives.empty()) {
    return {};
  }
  if (alternatives.size() == 1) {
    return alternatives.front();
  }

  std::string result = "(";
  for (std::size_t index = 0; index < alternatives.size(); ++index) {
    if (index > 0) {
      result += " | ";
    }
    result += quote_list_item(alternatives[index]);
  }
  result += ")";
  return result;
}

[[nodiscard]] std::string
format_expect_frontier(std::span<const parser::ExpectPath> frontier) {
  std::vector<std::string> alternatives;
  alternatives.reserve(frontier.size());
  for (const auto &path : frontier) {
    auto text = format_expect_path(path);
    if (!text.empty() &&
        std::ranges::find(alternatives, text) == alternatives.end()) {
      alternatives.push_back(std::move(text));
    }
  }
  return format_expect_alternatives(alternatives);
}

[[nodiscard]] TextOffset diagnostic_expect_offset(
    const workspace::Document &document,
    const parser::ParseDiagnostic &parseDiagnostic) noexcept {
  const auto textSize =
      static_cast<TextOffset>(document.textDocument().getText().size());
  auto offset = std::min(parseDiagnostic.offset, textSize);
  if ((parseDiagnostic.kind != parser::ParseDiagnosticKind::Inserted &&
       parseDiagnostic.kind != parser::ParseDiagnosticKind::Incomplete) ||
      document.parseResult.failureVisibleCursorOffset <= offset) {
    return offset;
  }

  const auto failureOffset =
      std::min(document.parseResult.failureVisibleCursorOffset, textSize);
  if (const auto leaf = next_visible_leaf_at_or_after(document, failureOffset);
      leaf.has_value()) {
    return leaf->getBegin() > failureOffset ? failureOffset : offset;
  }
  return failureOffset;
}

[[nodiscard]] FoundToken find_found_token(const workspace::Document &document,
                                          TextOffset offset) {
  if (const auto leaf = next_visible_leaf_at_or_after(document, offset);
      leaf.has_value()) {
    return {.begin = leaf->getBegin(), .end = leaf->getEnd(), .image = leaf->getText()};
  }

  const auto text = std::string_view{document.textDocument().getText()};
  const auto size = static_cast<TextOffset>(text.size());
  const auto begin = std::min(offset, size);
  return begin >= size
             ? FoundToken{.begin = size, .end = size, .image = {}}
             : FoundToken{.begin = begin,
                          .end = size,
                          .image = text.substr(static_cast<std::size_t>(begin))};
}

[[nodiscard]] std::string expect_text(
    const workspace::Document &document, const parser::Parser &parserImpl,
    TextOffset offset, const utils::CancellationToken &cancelToken) {
  const auto expect =
      parserImpl.expect(document.textDocument().getText(), offset, cancelToken);
  return format_expect_frontier(expect.frontier);
}

[[nodiscard]] TextOffset
previous_visible_leaf_end(const workspace::Document &document,
                          TextOffset offset) {
  if (document.parseResult.cst == nullptr) {
    return offset;
  }

  std::optional<CstNodeView> previousVisible;
  for (auto leaf = find_first_leaf(*document.parseResult.cst); leaf.has_value();
       leaf = find_next_leaf(*leaf)) {
    if (leaf->getBegin() >= offset) {
      break;
    }
    if (!leaf->isHidden()) {
      previousVisible = leaf;
    }
  }

  if (!previousVisible.has_value() || previousVisible->getEnd() >= offset) {
    return offset;
  }
  return previousVisible->getEnd();
}

[[nodiscard]] constexpr bool is_ascii_whitespace(char c) noexcept {
  switch (c) {
  case ' ':
  case '\t':
  case '\n':
  case '\r':
  case '\f':
  case '\v':
    return true;
  default:
    return false;
  }
}

[[nodiscard]] bool raw_text_gap_is_whitespace(const workspace::Document &document,
                                              TextOffset begin,
                                              TextOffset end) {
  if (begin >= end) {
    return true;
  }
  const auto text = std::string_view{document.textDocument().getText()};
  const auto safeBegin = std::min(begin, static_cast<TextOffset>(text.size()));
  const auto safeEnd = std::min(std::max(safeBegin, end),
                                static_cast<TextOffset>(text.size()));
  return std::ranges::all_of(
      text.substr(static_cast<std::size_t>(safeBegin),
                  static_cast<std::size_t>(safeEnd - safeBegin)),
      [](char c) { return is_ascii_whitespace(c); });
}

[[nodiscard]] bool gap_is_hidden_or_whitespace(const workspace::Document &document,
                                               TextOffset begin,
                                               TextOffset end) {
  if (begin >= end) {
    return true;
  }
  if (document.parseResult.cst == nullptr) {
    return raw_text_gap_is_whitespace(document, begin, end);
  }

  TextOffset coveredUntil = begin;
  for (auto leaf = find_first_leaf(*document.parseResult.cst); leaf.has_value();
       leaf = find_next_leaf(*leaf)) {
    if (leaf->getEnd() <= coveredUntil) {
      continue;
    }
    if (leaf->getBegin() >= end) {
      break;
    }
    if (leaf->getBegin() > coveredUntil &&
        !raw_text_gap_is_whitespace(document, coveredUntil, leaf->getBegin())) {
      return false;
    }
    if (!leaf->isHidden()) {
      return false;
    }
    coveredUntil = std::max(coveredUntil, std::min(leaf->getEnd(), end));
    if (coveredUntil >= end) {
      return true;
    }
  }
  return raw_text_gap_is_whitespace(document, coveredUntil, end);
}

[[nodiscard]] TextOffset zero_width_diagnostic_offset(
    const workspace::Document &document, TextOffset offset,
    const FoundToken &foundToken) {
  const auto textSize =
      static_cast<TextOffset>(document.textDocument().getText().size());
  const auto safeOffset = std::min(offset, textSize);
  if (document.parseResult.cst == nullptr) {
    return foundToken.image.empty() ? safeOffset
                                    : std::min(foundToken.begin, safeOffset);
  }
  const auto previousVisibleEnd = previous_visible_leaf_end(document, safeOffset);
  return gap_is_hidden_or_whitespace(document, previousVisibleEnd, safeOffset)
             ? previousVisibleEnd
             : safeOffset;
}

[[nodiscard]] TextOffset gap_insert_diagnostic_end(
    const workspace::Document &document, TextOffset begin,
    const FoundToken &foundToken) {
  const auto text = std::string_view{document.textDocument().getText()};
  const auto safeBegin =
      std::min(begin, static_cast<TextOffset>(text.size()));
  if (foundToken.image.empty() || foundToken.begin != safeBegin ||
      safeBegin >= static_cast<TextOffset>(text.size())) {
    return safeBegin;
  }
  const char *const cursor = text.data() + safeBegin;
  const char *const next = parser::detail::next_codepoint_cursor(cursor);
  return static_cast<TextOffset>(std::min<std::size_t>(
      text.size(), static_cast<std::size_t>(next - text.data())));
}

[[nodiscard]] pegium::Diagnostic
make_base_diagnostic(TextOffset begin, TextOffset end, std::string code) {
  pegium::Diagnostic diagnostic;
  diagnostic.severity = pegium::DiagnosticSeverity::Error;
  diagnostic.source = "parse";
  diagnostic.begin = begin;
  diagnostic.end = end;
  diagnostic.code = pegium::DiagnosticCode(std::move(code));
  return diagnostic;
}

[[nodiscard]] pegium::Diagnostic from_parse_diagnostic(
    const workspace::Document &document, const parser::Parser &parserImpl,
    const parser::ParseDiagnostic &parseDiagnostic,
    const utils::CancellationToken &cancelToken) {
  if (parseDiagnostic.kind == parser::ParseDiagnosticKind::ConversionError) {
    const auto textSize =
        static_cast<TextOffset>(document.textDocument().getText().size());
    const auto begin = std::min(parseDiagnostic.beginOffset, textSize);
    const auto end = std::min(std::max(begin, parseDiagnostic.endOffset), textSize);
    auto diagnostic = make_base_diagnostic(begin, end, "parse.conversion");
    diagnostic.message = parseDiagnostic.message.empty()
                             ? "Value conversion failed."
                             : parseDiagnostic.message;
    return diagnostic;
  }

  const auto expectOffset = diagnostic_expect_offset(document, parseDiagnostic);
  const auto foundToken = find_found_token(document, expectOffset);
  auto expected = format_expect_element(parseDiagnostic.element);
  if (expected.empty()) {
    expected = expect_text(document, parserImpl, expectOffset, cancelToken);
  }
  const auto unexpectedMessage =
      foundToken.image.empty()
          ? std::string("Unexpected end of input.")
          : "Unexpected token `" + std::string(foundToken.image) + "`.";

  auto zeroWidth =
      (parseDiagnostic.kind == parser::ParseDiagnosticKind::Inserted ||
       parseDiagnostic.kind == parser::ParseDiagnosticKind::Incomplete)
          ? zero_width_diagnostic_offset(document, expectOffset, foundToken)
          : std::min<TextOffset>(
                expectOffset,
                static_cast<TextOffset>(document.textDocument().getText().size()));
  auto diagnostic =
      make_base_diagnostic(zeroWidth, zeroWidth, "parse.incomplete");

  using enum parser::ParseDiagnosticKind;
  switch (parseDiagnostic.kind) {
  case Inserted:
    diagnostic.code = pegium::DiagnosticCode(std::string("parse.inserted"));
    diagnostic.message =
        !parseDiagnostic.message.empty()
            ? parseDiagnostic.message
            : (expected.empty() ? "Unexpected input."
                                : "Expecting " + expected);
    if (parseDiagnostic.element == nullptr && !parseDiagnostic.message.empty()) {
      diagnostic.end = gap_insert_diagnostic_end(document, diagnostic.begin,
                                                 foundToken);
    }
    maybe_attach_insert_code_action(diagnostic, parseDiagnostic, zeroWidth);
    break;
  case Deleted:
    if (parseDiagnostic.endOffset > parseDiagnostic.beginOffset) {
      const auto text = std::string_view{document.textDocument().getText()};
      const auto safeBegin = std::min(parseDiagnostic.beginOffset,
                                      static_cast<TextOffset>(text.size()));
      const auto safeEnd = std::min(std::max(safeBegin, parseDiagnostic.endOffset),
                                    static_cast<TextOffset>(text.size()));
      diagnostic = make_base_diagnostic(safeBegin, safeEnd, "parse.deleted");
      const auto deletedText =
          safeBegin < safeEnd
              ? text.substr(static_cast<std::size_t>(safeBegin),
                            static_cast<std::size_t>(safeEnd - safeBegin))
              : std::string_view{};
      diagnostic.message =
          deletedText.empty()
              ? "Unexpected end of input."
              : "Unexpected token `" + std::string(deletedText) + "`.";
    } else {
      diagnostic =
          make_base_diagnostic(foundToken.begin, foundToken.end, "parse.deleted");
      diagnostic.message = unexpectedMessage;
    }
    maybe_attach_delete_code_action(diagnostic);
    break;
  case Replaced:
    if (parseDiagnostic.endOffset > parseDiagnostic.beginOffset) {
      const auto text = std::string_view{document.textDocument().getText()};
      const auto safeBegin = std::min(parseDiagnostic.beginOffset,
                                      static_cast<TextOffset>(text.size()));
      const auto safeEnd = std::min(std::max(safeBegin, parseDiagnostic.endOffset),
                                    static_cast<TextOffset>(text.size()));
      diagnostic =
          make_base_diagnostic(safeBegin, safeEnd, "parse.replaced");
      const auto replacedText =
          safeBegin < safeEnd
              ? text.substr(static_cast<std::size_t>(safeBegin),
                            static_cast<std::size_t>(safeEnd - safeBegin))
              : std::string_view{};
      if (!parseDiagnostic.message.empty()) {
        diagnostic.message = parseDiagnostic.message;
      } else if (!expected.empty() && !replacedText.empty()) {
        diagnostic.message = "Expecting " + expected + " but found `" +
                             std::string(replacedText) + "`.";
      } else if (!expected.empty()) {
        diagnostic.message = "Expecting " + expected;
      } else if (!replacedText.empty()) {
        diagnostic.message =
            "Unexpected token `" + std::string(replacedText) + "`.";
      } else {
        diagnostic.message = unexpectedMessage;
      }
      maybe_attach_replace_code_action(diagnostic, parseDiagnostic, safeBegin,
                                       safeEnd);
      break;
    }
    diagnostic =
        make_base_diagnostic(foundToken.begin, foundToken.end, "parse.replaced");
    if (!parseDiagnostic.message.empty()) {
      diagnostic.message = parseDiagnostic.message;
    } else if (!expected.empty() && !foundToken.image.empty()) {
      diagnostic.message = "Expecting " + expected + " but found `" +
                           std::string(foundToken.image) + "`.";
    } else if (!expected.empty()) {
      diagnostic.message = "Expecting " + expected;
    } else {
      diagnostic.message = unexpectedMessage;
    }
    maybe_attach_replace_code_action(diagnostic, parseDiagnostic, foundToken.begin,
                                     foundToken.end);
    break;
  case Incomplete:
    if (parseDiagnostic.endOffset > parseDiagnostic.beginOffset) {
      const auto text = std::string_view{document.textDocument().getText()};
      const auto safeBegin = std::min(parseDiagnostic.beginOffset,
                                      static_cast<TextOffset>(text.size()));
      const auto safeEnd = std::min(std::max(safeBegin, parseDiagnostic.endOffset),
                                    static_cast<TextOffset>(text.size()));
      diagnostic =
          make_base_diagnostic(safeBegin, safeEnd, "parse.incomplete");
    }
    diagnostic.code = pegium::DiagnosticCode(std::string("parse.incomplete"));
    diagnostic.message =
        !parseDiagnostic.message.empty()
            ? parseDiagnostic.message
            : (expected.empty() ? unexpectedMessage : "Expecting " + expected);
    break;
  case Recovered:
    diagnostic.code = pegium::DiagnosticCode(std::string("parse.recovered"));
    diagnostic.message = "Recovered parse node";
    break;
  case ConversionError:
    break;
  }

  return diagnostic;
}

[[nodiscard]] std::vector<pegium::Diagnostic> extract_parse_diagnostics(
    const workspace::Document &document, const parser::Parser &parserImpl,
    const utils::CancellationToken &cancelToken) {
  std::vector<pegium::Diagnostic> diagnostics;
  diagnostics.reserve(document.parseResult.parseDiagnostics.size());

  for (const auto &parseDiagnostic : document.parseResult.parseDiagnostics) {
    diagnostics.push_back(from_parse_diagnostic(document, parserImpl,
                                                parseDiagnostic, cancelToken));
  }

  return diagnostics;
}

} // namespace

bool DefaultDocumentValidator::run_builtin_validation(
    const ValidationOptions &options) const noexcept {
  return options.categories.empty() ||
         std::ranges::find(options.categories, kBuiltInValidationCategory) !=
             options.categories.end();
}

bool DefaultDocumentValidator::run_custom_validation(
    const ValidationOptions &options) const noexcept {
  if (options.categories.empty()) {
    return true;
  }
  return std::ranges::any_of(options.categories,
                             [](const std::string &category) {
                               return category != kBuiltInValidationCategory;
                             });
}

void DefaultDocumentValidator::processParsingErrors(
    const workspace::Document &document,
    std::vector<pegium::Diagnostic> &diagnostics,
    const utils::CancellationToken &cancelToken) const {
  if (document.parseResult.parseDiagnostics.empty()) {
    return;
  }
  assert(services.parser != nullptr);
  auto parseDiagnostics =
      extract_parse_diagnostics(document, *services.parser, cancelToken);
  const auto& source = services.languageMetaData.languageId;
  for (auto &diagnostic : parseDiagnostics) {
    diagnostic.source = source;
  }
  diagnostics.insert(diagnostics.end(),
                     std::make_move_iterator(parseDiagnostics.begin()),
                     std::make_move_iterator(parseDiagnostics.end()));
}

void DefaultDocumentValidator::processLinkingErrors(
    const workspace::Document &document,
    std::vector<pegium::Diagnostic> &diagnostics, const std::string &source,
    const utils::CancellationToken &cancelToken) const {
  std::uint32_t cancelPollCounter = 0;
  for (const auto &handle : document.references) {
    if ((++cancelPollCounter & 0x3fU) == 0U) {
      utils::throw_if_cancelled(cancelToken);
    }
    const auto &reference = *handle.getConst();
    if (!reference.hasError()) {
      continue;
    }

    const auto &refText = reference.getRefText();
    if (refText.empty()) {
      continue;
    }

    TextOffset begin = 0;
    TextOffset end = static_cast<TextOffset>(refText.size());
    if (const auto refNode = reference.getRefNode(); refNode.has_value()) {
      begin = refNode->getBegin();
      end = refNode->getEnd();
    }

    diagnostics.push_back(pegium::Diagnostic{
        .severity = pegium::DiagnosticSeverity::Error,
        .message = "Unresolved reference: " + std::string(refText),
        .source = source,
        .code = pegium::DiagnosticCode(
            std::string("linking.unresolved-reference")),
        .data = std::nullopt,
        .begin = begin,
        .end = end});
  }
}

void DefaultDocumentValidator::validateAst(
    const AstNode &rootNode, std::vector<pegium::Diagnostic> &diagnostics,
    const ValidationOptions &options, const std::string &source,
    const utils::CancellationToken &cancelToken) const {
  const ValidationAcceptor acceptor{
      make_collecting_acceptor(diagnostics, source)};
  validateAstBefore(rootNode, acceptor, options.categories, cancelToken);
  validateAstNodes(rootNode, acceptor, options.categories, cancelToken);
  validateAstAfter(rootNode, acceptor, options.categories, cancelToken);
}

void DefaultDocumentValidator::validateAstBefore(
    const AstNode &rootNode, const ValidationAcceptor &acceptor,
    std::span<const std::string> categories,
    const utils::CancellationToken &cancelToken) const {
  const auto *validationRegistry =
      services.validation.validationRegistry.get();
  for (const auto &checkBefore : validationRegistry->checksBefore()) {
    utils::throw_if_cancelled(cancelToken);
    checkBefore(rootNode, acceptor, categories, cancelToken);
  }
}

void DefaultDocumentValidator::validateAstNodes(
    const AstNode &rootNode, const ValidationAcceptor &acceptor,
    std::span<const std::string> categories,
    const utils::CancellationToken &cancelToken) const {
  const auto *validationRegistry =
      services.validation.validationRegistry.get();
  auto preparedChecks = validationRegistry->prepareChecks(categories);
  preparedChecks->run(rootNode, acceptor, cancelToken);

  std::uint32_t cancelPollCounter = 0;
  for (const auto *node : rootNode.getAllContent()) {
    if ((++cancelPollCounter & 0x3fU) == 0U) {
      utils::throw_if_cancelled(cancelToken);
    }
    preparedChecks->run(*node, acceptor, cancelToken);
  }
}

void DefaultDocumentValidator::validateAstAfter(
    const AstNode &rootNode, const ValidationAcceptor &acceptor,
    std::span<const std::string> categories,
    const utils::CancellationToken &cancelToken) const {
  const auto *validationRegistry =
      services.validation.validationRegistry.get();
  for (const auto &checkAfter : validationRegistry->checksAfter()) {
    utils::throw_if_cancelled(cancelToken);
    checkAfter(rootNode, acceptor, categories, cancelToken);
  }
}

std::vector<pegium::Diagnostic> DefaultDocumentValidator::validateDocument(
    const workspace::Document &document, const ValidationOptions &options,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);

  std::vector<pegium::Diagnostic> diagnostics;
  const auto& source = services.languageMetaData.languageId;

  if (run_builtin_validation(options)) {
    const auto parsingDiagnosticCount = diagnostics.size();
    processParsingErrors(document, diagnostics, cancelToken);
    if (options.stopAfterParsingErrors.value_or(false) &&
        diagnostics.size() > parsingDiagnosticCount) {
      return diagnostics;
    }

    const auto linkingDiagnosticCount = diagnostics.size();
    processLinkingErrors(document, diagnostics, source, cancelToken);
    if (options.stopAfterLinkingErrors.value_or(false) &&
        diagnostics.size() > linkingDiagnosticCount) {
      return diagnostics;
    }
  }

  if (!document.hasAst() || !run_custom_validation(options)) {
    return diagnostics;
  }

  validateAst(*document.parseResult.value, diagnostics, options, source,
              cancelToken);

  utils::throw_if_cancelled(cancelToken);
  return diagnostics;
}

} // namespace pegium::validation
