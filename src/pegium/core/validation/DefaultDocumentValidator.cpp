#include <pegium/core/validation/DefaultDocumentValidator.hpp>

#include <algorithm>
#include <cctype>
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
#include <pegium/core/parser/TextUtils.hpp>
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

[[nodiscard]] FoundToken find_found_token(const workspace::Document &document,
                                          TextOffset offset);

ValidationAcceptor
make_collecting_acceptor(std::vector<services::Diagnostic> &diagnostics,
                         const std::string &source) {
  return ValidationAcceptor{
      [&diagnostics, &source](services::Diagnostic diagnostic) {
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

void append_default_code_action_data(services::Diagnostic &diagnostic,
                                     std::string editKind, std::string title,
                                     TextOffset begin, TextOffset end,
                                     std::string newText) {
  services::JsonValue::Object action;
  action.try_emplace("kind", "quickfix");
  action.try_emplace("editKind", std::move(editKind));
  action.try_emplace("title", std::move(title));
  action.try_emplace("begin", static_cast<std::int64_t>(begin));
  action.try_emplace("end", static_cast<std::int64_t>(end));
  action.try_emplace("newText", std::move(newText));

  services::JsonValue::Array actions;
  actions.emplace_back(std::move(action));

  services::JsonValue::Object data;
  data.try_emplace(std::string(kDefaultCodeActionsKey), std::move(actions));
  diagnostic.data = services::JsonValue(std::move(data));
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
    const bool keywordLike =
        std::ranges::all_of(value, [](char c) { return parser::isWord(c); });
    return keywordLike ? quote_keyword(value) : std::string(value);
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

void maybe_attach_insert_code_action(services::Diagnostic &diagnostic,
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

void maybe_attach_replace_code_action(services::Diagnostic &diagnostic,
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

void maybe_attach_delete_code_action(services::Diagnostic &diagnostic) {
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

[[nodiscard]] std::vector<std::string>
collect_expected_texts(std::span<const parser::ParseDiagnostic> diagnostics) {
  std::vector<std::string> alternatives;
  alternatives.reserve(diagnostics.size());
  for (const auto &diagnostic : diagnostics) {
    auto text = format_expect_element(diagnostic.element);
    if (!text.empty() &&
        std::ranges::find(alternatives, text) == alternatives.end()) {
      alternatives.push_back(std::move(text));
    }
  }
  return alternatives;
}

[[nodiscard]] bool is_word_token(std::string_view image) {
  return !image.empty() &&
         std::ranges::all_of(image, [](char c) { return parser::isWord(c); });
}

[[nodiscard]] bool starts_with_ignore_case(std::string_view text,
                                           std::string_view prefix) {
  if (text.size() < prefix.size()) {
    return false;
  }
  for (std::size_t index = 0; index < prefix.size(); ++index) {
    if (parser::tolower(text[index]) != parser::tolower(prefix[index])) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] std::vector<parser::ExpectPath> narrow_replaced_frontier(
    const FoundToken &foundToken, std::span<const parser::ExpectPath> frontier) {
  if (!is_word_token(foundToken.image)) {
    return {};
  }

  std::vector<parser::ExpectPath> narrowed;
  for (const auto &path : frontier) {
    const auto *literal = path.literal();
    if (literal == nullptr) {
      continue;
    }
    const auto value = literal->getValue();
    if (!is_word_token(value) || value.size() <= foundToken.image.size()) {
      continue;
    }
    if (starts_with_ignore_case(value, foundToken.image)) {
      narrowed.push_back(path);
    }
  }
  return narrowed;
}

[[nodiscard]] bool is_word_like_expect_path(const parser::ExpectPath &path) {
  if (const auto *literal = path.literal(); literal != nullptr) {
    return is_word_token(literal->getValue());
  }
  if (path.expectedReferenceAssignment() != nullptr ||
      path.expectedRule() != nullptr) {
    return true;
  }
  return false;
}

[[nodiscard]] bool is_punctuation_expect_path(const parser::ExpectPath &path) {
  const auto *literal = path.literal();
  if (literal == nullptr) {
    return false;
  }
  const auto value = literal->getValue();
  return !value.empty() && !is_word_token(value);
}

[[nodiscard]] bool is_continuation_only_frontier(
    std::span<const parser::ExpectPath> frontier) {
  return !frontier.empty() &&
         std::ranges::none_of(frontier, is_word_like_expect_path);
}

[[nodiscard]] std::vector<parser::ExpectPath>
prefer_word_like_frontier(std::span<const parser::ExpectPath> frontier) {
  std::vector<parser::ExpectPath> wordLike;
  bool hasPunctuation = false;
  for (const auto &path : frontier) {
    if (is_word_like_expect_path(path)) {
      wordLike.push_back(path);
      continue;
    }
    if (is_punctuation_expect_path(path)) {
      hasPunctuation = true;
    }
  }
  if (!wordLike.empty() && hasPunctuation) {
    return wordLike;
  }
  return {};
}

[[nodiscard]] std::span<const parser::ExpectPath>
select_expect_frontier_for_diagnostic(
    const parser::ParseDiagnostic &parseDiagnostic, const FoundToken &foundToken,
    std::span<const parser::ExpectPath> frontier,
    std::vector<parser::ExpectPath> &scratch) {
  if (parseDiagnostic.kind == parser::ParseDiagnosticKind::Replaced) {
    scratch = narrow_replaced_frontier(foundToken, frontier);
    if (!scratch.empty()) {
      return scratch;
    }
  }

  if (const bool zeroWidthGap =
          foundToken.image.empty() || foundToken.begin > parseDiagnostic.offset;
      (parseDiagnostic.kind == parser::ParseDiagnosticKind::Inserted ||
       parseDiagnostic.kind == parser::ParseDiagnosticKind::Incomplete) &&
      zeroWidthGap) {
    scratch = prefer_word_like_frontier(frontier);
    if (!scratch.empty()) {
      return scratch;
    }
  }

  return frontier;
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
  const auto failureToken = find_found_token(document, failureOffset);
  return failureToken.image.empty() ? failureOffset : offset;
}

[[nodiscard]] FoundToken find_found_token(const workspace::Document &document,
                                          TextOffset offset) {
  const auto text = document.textDocument().getText();
  const auto size = static_cast<TextOffset>(text.size());
  TextOffset cursor = std::min(offset, size);
  while (cursor < size &&
         std::isspace(static_cast<unsigned char>(text[cursor])) != 0) {
    ++cursor;
  }
  if (cursor >= size) {
    return {.begin = size, .end = size, .image = {}};
  }

  const auto begin = cursor;
  if (parser::isWord(text[cursor])) {
    ++cursor;
    while (cursor < size && parser::isWord(text[cursor])) {
      ++cursor;
    }
    return {.begin = begin,
            .end = cursor,
            .image = text.substr(static_cast<std::size_t>(begin),
                                 static_cast<std::size_t>(cursor - begin))};
  }

  const auto *data = text.data();
  const auto *end = parser::advanceOneCodepointLossy(data + cursor);
  cursor = static_cast<TextOffset>(end - data);
  return {.begin = begin,
          .end = cursor,
          .image = text.substr(static_cast<std::size_t>(begin),
                               static_cast<std::size_t>(cursor - begin))};
}

[[nodiscard]] std::string expect_text(
    const workspace::Document &document, const parser::Parser &parserImpl,
    TextOffset offset, const utils::CancellationToken &cancelToken,
    const parser::ParseDiagnostic &parseDiagnostic, const FoundToken &foundToken) {
  const auto expect =
      parserImpl.expect(document.textDocument().getText(), offset, cancelToken);
  std::vector<parser::ExpectPath> scratch;
  return format_expect_frontier(select_expect_frontier_for_diagnostic(
      parseDiagnostic, foundToken, expect.frontier, scratch));
}

[[nodiscard]] bool
is_punctuation_literal(const grammar::AbstractElement *element) noexcept {
  if (element == nullptr ||
      element->getKind() != grammar::ElementKind::Literal) {
    return false;
  }
  const auto value = static_cast<const grammar::Literal *>(element)->getValue();
  return !value.empty() &&
         !std::ranges::all_of(value, [](char c) { return parser::isWord(c); });
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

[[nodiscard]] TextOffset skip_trivia_forward(const workspace::Document &document,
                                             TextOffset offset) {
  const auto text = document.textDocument().getText();
  const auto size = static_cast<TextOffset>(text.size());
  auto cursor = std::min(offset, size);
  while (cursor < size &&
         std::isspace(static_cast<unsigned char>(text[cursor])) != 0) {
    ++cursor;
  }
  return cursor;
}

[[nodiscard]] bool trivia_only_between(const workspace::Document &document,
                                       TextOffset begin, TextOffset end) {
  const auto text = document.textDocument().getText();
  const auto size = static_cast<TextOffset>(text.size());
  const auto safeBegin = std::min(begin, size);
  const auto safeEnd = std::min(std::max(safeBegin, end), size);
  for (auto offset = safeBegin; offset < safeEnd; ++offset) {
    if (std::isspace(static_cast<unsigned char>(text[offset])) == 0) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] TextOffset punctuation_diagnostic_offset(
    const workspace::Document &document,
    const parser::ParseDiagnostic &parseDiagnostic) {
  const auto offset = std::min<TextOffset>(
      parseDiagnostic.offset,
      static_cast<TextOffset>(document.textDocument().getText().size()));
  if (!is_punctuation_literal(parseDiagnostic.element)) {
    return offset;
  }
  return previous_visible_leaf_end(document, offset);
}

[[nodiscard]] bool should_merge_inserted(
    std::span<const parser::ParseDiagnostic> diagnostics) {
  if (diagnostics.size() < 2) {
    return false;
  }
  return std::ranges::all_of(diagnostics, [&diagnostics](const auto &diagnostic) {
    return diagnostic.kind == parser::ParseDiagnosticKind::Inserted &&
           diagnostic.offset == diagnostics.front().offset;
  });
}

[[nodiscard]] bool should_merge_deleted(
    std::span<const parser::ParseDiagnostic> diagnostics) {
  return diagnostics.size() > 1 &&
         std::ranges::all_of(diagnostics, [](const auto &diagnostic) {
           return diagnostic.kind == parser::ParseDiagnosticKind::Deleted;
         });
}

[[nodiscard]] services::Diagnostic
make_base_diagnostic(TextOffset begin, TextOffset end, std::string code) {
  services::Diagnostic diagnostic;
  diagnostic.severity = services::DiagnosticSeverity::Error;
  diagnostic.source = "parse";
  diagnostic.begin = begin;
  diagnostic.end = end;
  diagnostic.code = services::DiagnosticCode(std::move(code));
  return diagnostic;
}

[[nodiscard]] services::Diagnostic from_parse_diagnostic(
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
    expected = expect_text(document, parserImpl, expectOffset, cancelToken,
                           parseDiagnostic, foundToken);
  }
  const auto unexpectedMessage =
      foundToken.image.empty()
          ? std::string("Unexpected end of input.")
          : "Unexpected token `" + std::string(foundToken.image) + "`.";

  auto zeroWidth =
      (parseDiagnostic.kind == parser::ParseDiagnosticKind::Inserted ||
       parseDiagnostic.kind == parser::ParseDiagnosticKind::Incomplete)
          ? punctuation_diagnostic_offset(document, parseDiagnostic)
          : std::min<TextOffset>(
                expectOffset,
                static_cast<TextOffset>(document.textDocument().getText().size()));
  if ((parseDiagnostic.kind == parser::ParseDiagnosticKind::Inserted ||
       parseDiagnostic.kind == parser::ParseDiagnosticKind::Incomplete) &&
      foundToken.image.empty()) {
    const auto visibleAnchor = previous_visible_leaf_end(document, expectOffset);
    zeroWidth = trivia_only_between(document, visibleAnchor, expectOffset)
                    ? visibleAnchor
                    : expectOffset;
  }
  auto diagnostic =
      make_base_diagnostic(zeroWidth, zeroWidth, "parse.incomplete");

  using enum parser::ParseDiagnosticKind;
  switch (parseDiagnostic.kind) {
  case Inserted:
    diagnostic.code = services::DiagnosticCode(std::string("parse.inserted"));
    diagnostic.message = expected.empty() ? "Unexpected input."
                                          : "Expecting " + expected;
    maybe_attach_insert_code_action(diagnostic, parseDiagnostic, zeroWidth);
    break;
  case Deleted:
    diagnostic =
        make_base_diagnostic(foundToken.begin, foundToken.end, "parse.deleted");
    diagnostic.message = unexpectedMessage;
    maybe_attach_delete_code_action(diagnostic);
    break;
  case Replaced:
    diagnostic =
        make_base_diagnostic(foundToken.begin, foundToken.end, "parse.replaced");
    if (!expected.empty() && !foundToken.image.empty()) {
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
    diagnostic.code = services::DiagnosticCode(std::string("parse.incomplete"));
    diagnostic.message =
        expected.empty() ? unexpectedMessage : "Expecting " + expected;
    break;
  case Recovered:
    diagnostic.code = services::DiagnosticCode(std::string("parse.recovered"));
    diagnostic.message = "Recovered parse node";
    break;
  case ConversionError:
    break;
  }

  return diagnostic;
}

[[nodiscard]] bool should_skip_cascade_inserted(
    const workspace::Document &document, const parser::Parser &parserImpl,
    std::span<const parser::ParseDiagnostic> diagnostics, std::size_t index,
    const utils::CancellationToken &cancelToken) {
  if (index == 0 || index >= diagnostics.size()) {
    return false;
  }

  const auto &current = diagnostics[index];
  const auto &previous = diagnostics[index - 1];
  if (current.kind != parser::ParseDiagnosticKind::Inserted ||
      previous.kind != parser::ParseDiagnosticKind::Replaced) {
    return false;
  }

  const auto previousFound = find_found_token(document, previous.offset);
  if (const auto anchoredCurrentOffset =
          skip_trivia_forward(document, previousFound.end);
      !is_word_token(previousFound.image) ||
      current.offset != anchoredCurrentOffset) {
    return false;
  }

  const auto expect =
      parserImpl.expect(document.textDocument().getText(), current.offset,
                        cancelToken);
  const auto currentFound = find_found_token(document, current.offset);
  std::vector<parser::ExpectPath> scratch;
  const auto selected = select_expect_frontier_for_diagnostic(
      current, currentFound, expect.frontier, scratch);
  return is_continuation_only_frontier(selected);
}

[[nodiscard]] services::Diagnostic
from_deleted_run(const workspace::Document &document,
                 std::span<const parser::ParseDiagnostic> diagnostics) {
  const auto begin = diagnostics.front().offset;
  const auto lastToken = find_found_token(document, diagnostics.back().offset);
  const auto end = std::max(begin, lastToken.end);
  auto diagnostic = make_base_diagnostic(begin, end, "parse.deleted");

  const auto text = std::string_view{document.textDocument().getText()};
  const auto safeBegin = std::min(begin, static_cast<TextOffset>(text.size()));
  const auto safeEnd = std::min(end, static_cast<TextOffset>(text.size()));
  const auto deletedText =
      safeBegin < safeEnd
          ? text.substr(static_cast<std::size_t>(safeBegin),
                        static_cast<std::size_t>(safeEnd - safeBegin))
                          : std::string_view{};
  diagnostic.message =
      deletedText.empty()
          ? "Unexpected end of input."
          : "Unexpected token `" + std::string(deletedText) + "`.";
  maybe_attach_delete_code_action(diagnostic);
  return diagnostic;
}

[[nodiscard]] services::Diagnostic
from_inserted_run(const workspace::Document &document,
                  const parser::Parser &parserImpl,
                  std::span<const parser::ParseDiagnostic> diagnostics,
                  const utils::CancellationToken &cancelToken) {
  const auto offset =
      punctuation_diagnostic_offset(document, diagnostics.front());
  const auto foundToken = find_found_token(document, diagnostics.front().offset);
  auto diagnostic = make_base_diagnostic(offset, offset, "parse.inserted");
  const auto expectedTexts = collect_expected_texts(diagnostics);
  const auto expected =
      expectedTexts.size() != 1
          ? expect_text(document, parserImpl, diagnostics.front().offset,
                        cancelToken, diagnostics.front(), foundToken)
          : format_expect_alternatives(expectedTexts);
  diagnostic.message =
      expected.empty() ? "Unexpected input." : "Expecting " + expected;
  return diagnostic;
}

[[nodiscard]] std::vector<services::Diagnostic> extract_parse_diagnostics(
    const workspace::Document &document, const parser::Parser &parserImpl,
    const utils::CancellationToken &cancelToken) {
  const auto parseDiagnostics = std::span(document.parseResult.parseDiagnostics);
  std::vector<services::Diagnostic> diagnostics;
  diagnostics.reserve(parseDiagnostics.size());

  for (std::size_t index = 0; index < parseDiagnostics.size();) {
    if (should_skip_cascade_inserted(document, parserImpl, parseDiagnostics, index,
                                     cancelToken)) {
      const auto offset = parseDiagnostics[index].offset;
      while (index < parseDiagnostics.size() &&
             parseDiagnostics[index].kind ==
                 parser::ParseDiagnosticKind::Inserted &&
             parseDiagnostics[index].offset == offset) {
        ++index;
      }
      continue;
    }

    std::size_t runEnd = index + 1;
    while (runEnd < parseDiagnostics.size()) {
      if (const auto run = parseDiagnostics.subspan(index, runEnd - index + 1);
          should_merge_inserted(run) || should_merge_deleted(run)) {
        ++runEnd;
        continue;
      }
      break;
    }

    const auto run = parseDiagnostics.subspan(index, runEnd - index);
    if (should_merge_inserted(run)) {
      diagnostics.push_back(
          from_inserted_run(document, parserImpl, run, cancelToken));
      index = runEnd;
      continue;
    }
    if (should_merge_deleted(run)) {
      diagnostics.push_back(from_deleted_run(document, run));
      index = runEnd;
      continue;
    }

    diagnostics.push_back(
        from_parse_diagnostic(document, parserImpl, parseDiagnostics[index],
                              cancelToken));
    ++index;
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
    std::vector<services::Diagnostic> &diagnostics,
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
    std::vector<services::Diagnostic> &diagnostics, const std::string &source,
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

    diagnostics.push_back(services::Diagnostic{
        .severity = services::DiagnosticSeverity::Error,
        .message = "Unresolved reference: " + std::string(refText),
        .source = source,
        .code = services::DiagnosticCode(
            std::string("linking.unresolved-reference")),
        .data = std::nullopt,
        .begin = begin,
        .end = end});
  }
}

void DefaultDocumentValidator::validateAst(
    const AstNode &rootNode, std::vector<services::Diagnostic> &diagnostics,
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

std::vector<services::Diagnostic> DefaultDocumentValidator::validateDocument(
    const workspace::Document &document, const ValidationOptions &options,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);

  std::vector<services::Diagnostic> diagnostics;
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
