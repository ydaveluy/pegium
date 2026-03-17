#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/AbstractFormatter.hpp>
#include <pegium/parser/PegiumParser.hpp>

namespace pegium::lsp {
namespace {

using namespace pegium::parser;

namespace mini::ast {

struct Field : pegium::AstNode {
  bool many = false;
  string name;
  string type;
};

struct Item : pegium::AstNode {
  string name;
  vector<pointer<Field>> fields;
};

struct Model : pegium::AstNode {
  vector<pointer<Item>> items;
};

} // namespace mini::ast

class MiniParser final : public parser::PegiumParser {
public:
  using PegiumParser::PegiumParser;
  using PegiumParser::parse;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return ModelRule;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Skipper skipper =
      SkipperBuilder().ignore(WS).hide(ML_COMMENT, SL_COMMENT).build();

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};

  Rule<mini::ast::Field> FieldRule{
      "Field",
      option(enable_if<&mini::ast::Field::many>("many"_kw.i())) +
          assign<&mini::ast::Field::name>(ID) + ":"_kw +
          assign<&mini::ast::Field::type>(ID)};

  Rule<mini::ast::Item> ItemRule{
      "Item",
      "entity"_kw.i() + assign<&mini::ast::Item::name>(ID) + "{"_kw +
          many(append<&mini::ast::Item::fields>(FieldRule)) + "}"_kw};

  Rule<mini::ast::Model> ModelRule{
      "Model", some(append<&mini::ast::Model::items>(ItemRule))};
#pragma clang diagnostic pop
};

const services::Services &lookup_services(const services::SharedServices &shared,
                                          std::string_view languageId) {
  const auto *coreServices =
      shared.serviceRegistry->getServicesByLanguageId(std::string(languageId));
  if (coreServices == nullptr) {
    throw std::runtime_error("Missing language services for formatter test.");
  }
  const auto *services = dynamic_cast<const services::Services *>(coreServices);
  if (services == nullptr) {
    throw std::runtime_error("Invalid formatter test services type.");
  }
  return *services;
}

const mini::ast::Model *model_of(const workspace::Document &document) {
  return pegium::ast_ptr_cast<mini::ast::Model>(document.parseResult.value);
}

struct ParsedMiniDocument {
  std::unique_ptr<services::SharedServices> shared;
  std::shared_ptr<workspace::Document> document;
};

ParsedMiniDocument open_mini_document(std::string text) {
  auto shared = test::make_shared_services();
  auto services = test::make_services<MiniParser>(*shared, "mini", {".mini"});
  if (!shared->serviceRegistry->registerServices(std::move(services))) {
    throw std::runtime_error("Failed to register mini formatter services.");
  }

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("mini-format.test"), "mini", std::move(text));
  if (document == nullptr) {
    throw std::runtime_error("Failed to parse mini formatter document.");
  }
  return ParsedMiniDocument{
      .shared = std::move(shared),
      .document = std::move(document),
  };
}

std::string apply_text_edits(const workspace::Document &document,
                             std::vector<::lsp::TextEdit> edits) {
  auto text = document.text();
  std::sort(edits.begin(), edits.end(),
            [&document](const auto &left, const auto &right) {
              return document.positionToOffset(left.range.start) >
                     document.positionToOffset(right.range.start);
            });

  for (const auto &edit : edits) {
    const auto begin = document.positionToOffset(edit.range.start);
    const auto end = document.positionToOffset(edit.range.end);
    text.replace(begin, end - begin, edit.newText);
  }
  return text;
}

class CountingFormatter final : public AbstractFormatter {
public:
  using AbstractFormatter::AbstractFormatter;

  mutable std::size_t callCount = 0;

protected:
  void format(FormattingBuilder &, const AstNode *) const override { ++callCount; }
};

class KeywordSpacingFormatter final : public AbstractFormatter {
public:
  explicit KeywordSpacingFormatter(const services::Services &services)
      : AbstractFormatter(services) {
    on<mini::ast::Item>(&KeywordSpacingFormatter::formatItem);
  }

protected:
  virtual void formatItem(FormattingBuilder &builder,
                          const mini::ast::Item *item) const {
    auto formatter = builder.getNodeFormatter(item);
    formatter.keyword("entity").append(oneSpace);
  }
};

class PriorityFormatter final : public AbstractFormatter {
public:
  explicit PriorityFormatter(const services::Services &services)
      : AbstractFormatter(services) {
    on<mini::ast::Item>(&PriorityFormatter::formatItem);
  }

protected:
  virtual void formatItem(FormattingBuilder &builder,
                          const mini::ast::Item *item) const {
    auto formatter = builder.getNodeFormatter(item);
    formatter.keyword("entity").append(spaces(3, {.priority = 0}));
    formatter.keyword("entity").append(oneSpace({.priority = 2}));
  }
};

class OverlapFormatter final : public AbstractFormatter {
public:
  explicit OverlapFormatter(const services::Services &services)
      : AbstractFormatter(services) {
    on<mini::ast::Item>(&OverlapFormatter::formatItem);
  }

protected:
  virtual void formatItem(FormattingBuilder &builder,
                          const mini::ast::Item *item) const {
    auto formatter = builder.getNodeFormatter(item);
    formatter.keyword("entity").append(spaces(3));
    formatter.property<&mini::ast::Item::name>().prepend(oneSpace);
  }
};

class FlexibleWhitespaceFormatter final : public AbstractFormatter {
public:
  explicit FlexibleWhitespaceFormatter(const services::Services &services)
      : AbstractFormatter(services) {
    on<mini::ast::Item>(&FlexibleWhitespaceFormatter::formatItem);
  }

protected:
  virtual void formatItem(FormattingBuilder &builder,
                          const mini::ast::Item *item) const {
    auto formatter = builder.getNodeFormatter(item);
    formatter.keyword("entity").append(oneSpace({.allowMore = true}));
    formatter.keyword("{").prepend(newLine({.allowLess = true}));
  }
};

class FitFormatter final : public AbstractFormatter {
public:
  explicit FitFormatter(const services::Services &services)
      : AbstractFormatter(services) {
    on<mini::ast::Item>(&FitFormatter::formatItem);
  }

protected:
  virtual void formatItem(FormattingBuilder &builder,
                          const mini::ast::Item *item) const {
    auto formatter = builder.getNodeFormatter(item);
    formatter.keyword("{").prepend(fit(oneSpace, newLine));
  }
};

class MiniLayoutFormatter final : public AbstractFormatter {
public:
  explicit MiniLayoutFormatter(const services::Services &services)
      : AbstractFormatter(services) {
    on<mini::ast::Model>(&MiniLayoutFormatter::formatModel);
    on<mini::ast::Item>(&MiniLayoutFormatter::formatItem);
    on<mini::ast::Field>(&MiniLayoutFormatter::formatField);
  }

protected:
  virtual void formatModel(FormattingBuilder &builder,
                           const mini::ast::Model *model) const {
    auto formatter = builder.getNodeFormatter(model);
    formatter.properties<&mini::ast::Model::items>().prepend(noIndent);
  }

  virtual void formatItem(FormattingBuilder &builder,
                          const mini::ast::Item *item) const {
    auto formatter = builder.getNodeFormatter(item);
    formatter.keyword("entity").append(oneSpace);
    const auto openBrace = formatter.keyword("{");
    const auto closeBrace = formatter.keyword("}");
    formatBlock(openBrace, closeBrace, formatter.interior(openBrace, closeBrace));
  }

  virtual void formatField(FormattingBuilder &builder,
                           const mini::ast::Field *field) const {
    auto formatter = builder.getNodeFormatter(field);
    if (field->many) {
      formatter.keyword("many").append(oneSpace);
    }
    formatter.keyword(":").prepend(noSpace).append(oneSpace);
  }
};

class HiddenSelectionFormatter final : public AbstractFormatter {
public:
  explicit HiddenSelectionFormatter(const services::Services &services)
      : AbstractFormatter(services) {
    on<mini::ast::Model>(&HiddenSelectionFormatter::formatModel);
    on<mini::ast::Item>(&HiddenSelectionFormatter::formatItem);
    on<mini::ast::Field>(&HiddenSelectionFormatter::formatField);
  }

protected:
  virtual void formatModel(FormattingBuilder &builder,
                           const mini::ast::Model *model) const {
    auto formatter = builder.getNodeFormatter(model);
    formatter.properties<&mini::ast::Model::items>().prepend(noIndent);
  }

  virtual void formatItem(FormattingBuilder &builder,
                          const mini::ast::Item *item) const {
    auto formatter = builder.getNodeFormatter(item);
    formatter.keyword("entity").append(oneSpace);
    const auto openBrace = formatter.keyword("{");
    const auto closeBrace = formatter.keyword("}");
    formatBlock(openBrace, closeBrace, formatter.properties<&mini::ast::Item::fields>(),
                {.beforeOpen = oneSpace, .insideEmpty = noSpace,
                 .afterOpen = newLine, .beforeClose = newLine,
                 .contentIndent = indent});
    formatter.hiddens("SL_COMMENT").prepend(indent);
  }

  virtual void formatField(FormattingBuilder &builder,
                           const mini::ast::Field *field) const {
    auto formatter = builder.getNodeFormatter(field);
    if (field->many) {
      formatter.keyword("many").append(oneSpace);
    }
    formatter.keyword(":").prepend(noSpace).append(oneSpace);
  }
};

class HiddenReplacementFormatter final : public AbstractFormatter {
public:
  explicit HiddenReplacementFormatter(const services::Services &services)
      : AbstractFormatter(services) {
    on<mini::ast::Model>(&HiddenReplacementFormatter::formatModel);
    on<mini::ast::Item>(&HiddenReplacementFormatter::formatItem);
    on<mini::ast::Field>(&HiddenReplacementFormatter::formatField);
    onHidden("ML_COMMENT", &HiddenReplacementFormatter::formatComment);
  }

protected:
  virtual void formatModel(FormattingBuilder &builder,
                           const mini::ast::Model *model) const {
    auto formatter = builder.getNodeFormatter(model);
    formatter.properties<&mini::ast::Model::items>().prepend(noIndent);
  }

  virtual void formatItem(FormattingBuilder &builder,
                          const mini::ast::Item *item) const {
    auto formatter = builder.getNodeFormatter(item);
    formatter.keyword("entity").append(oneSpace);
    const auto openBrace = formatter.keyword("{");
    const auto closeBrace = formatter.keyword("}");
    formatBlock(openBrace, closeBrace, formatter.interior(openBrace, closeBrace));
  }

  virtual void formatField(FormattingBuilder &builder,
                           const mini::ast::Field *field) const {
    auto formatter = builder.getNodeFormatter(field);
    if (field->many) {
      formatter.keyword("many").append(oneSpace);
    }
    formatter.keyword(":").prepend(noSpace).append(oneSpace);
  }

  virtual void formatComment(HiddenNodeFormatter &comment) const {
    comment.replace("/* level " + std::to_string(comment.baseIndentation()) +
                    "\n * updated\n */");
  }
};

class ExactDispatchFormatter final : public AbstractFormatter {
public:
  explicit ExactDispatchFormatter(const services::Services &services)
      : AbstractFormatter(services) {
    on<AstNode>(&ExactDispatchFormatter::formatAstNode);
    on<mini::ast::Model>(&ExactDispatchFormatter::formatModel);
    on<mini::ast::Item>(&ExactDispatchFormatter::formatItem);
    on<mini::ast::Field>(&ExactDispatchFormatter::formatField);
  }

  mutable std::size_t astNodeCount = 0;
  mutable std::size_t modelCount = 0;
  mutable std::size_t itemCount = 0;
  mutable std::size_t fieldCount = 0;

protected:
  virtual void formatAstNode(FormattingBuilder &, const AstNode *) const {
    ++astNodeCount;
  }
  virtual void formatModel(FormattingBuilder &,
                           const mini::ast::Model *) const {
    ++modelCount;
  }
  virtual void formatItem(FormattingBuilder &,
                          const mini::ast::Item *) const {
    ++itemCount;
  }
  virtual void formatField(FormattingBuilder &,
                           const mini::ast::Field *) const {
    ++fieldCount;
  }
};

class OverridingRegistrationFormatter final : public AbstractFormatter {
public:
  explicit OverridingRegistrationFormatter(const services::Services &services)
      : AbstractFormatter(services) {
    on<mini::ast::Item>(&OverridingRegistrationFormatter::formatItemWithThreeSpaces);
    on<mini::ast::Item>(&OverridingRegistrationFormatter::formatItemWithOneSpace);
  }

protected:
  virtual void formatItemWithThreeSpaces(FormattingBuilder &builder,
                                         const mini::ast::Item *item) const {
    builder.getNodeFormatter(item).keyword("entity").append(spaces(3));
  }

  virtual void formatItemWithOneSpace(FormattingBuilder &builder,
                                      const mini::ast::Item *item) const {
    builder.getNodeFormatter(item).keyword("entity").append(oneSpace);
  }
};

class OverridingHiddenRegistrationFormatter final : public AbstractFormatter {
public:
  explicit OverridingHiddenRegistrationFormatter(
      const services::Services &services)
      : AbstractFormatter(services) {
    on<mini::ast::Model>(&OverridingHiddenRegistrationFormatter::formatModel);
    on<mini::ast::Item>(&OverridingHiddenRegistrationFormatter::formatItem);
    onHidden("ML_COMMENT", &OverridingHiddenRegistrationFormatter::formatCommentFirst);
    onHidden("ML_COMMENT", &OverridingHiddenRegistrationFormatter::formatCommentSecond);
  }

protected:
  virtual void formatModel(FormattingBuilder &builder,
                           const mini::ast::Model *model) const {
    builder.getNodeFormatter(model)
        .properties<&mini::ast::Model::items>()
        .prepend(noIndent);
  }

  virtual void formatItem(FormattingBuilder &builder,
                          const mini::ast::Item *item) const {
    auto formatter = builder.getNodeFormatter(item);
    const auto openBrace = formatter.keyword("{");
    const auto closeBrace = formatter.keyword("}");
    formatBlock(openBrace, closeBrace, formatter.interior(openBrace, closeBrace));
  }

  virtual void formatCommentFirst(HiddenNodeFormatter &comment) const {
    comment.replace("/** first */");
  }

  virtual void formatCommentSecond(HiddenNodeFormatter &comment) const {
    comment.replace(
        formatMultilineComment(comment, {.forceMultiline = true, .maxBlankLines = 0}));
  }
};

class FormattingApiProbe final : public AbstractFormatter {
public:
  using AbstractFormatter::AbstractFormatter;

  [[nodiscard]] static FormattingAction oneSpaceAction(
      FormattingActionOptions options = {}) {
    return oneSpace(std::move(options));
  }

  [[nodiscard]] static std::string
  formatComment(std::string_view text,
                MultilineCommentFormatOptions options = {}) {
    return formatMultilineComment(text, std::move(options));
  }

  [[nodiscard]] static std::string
  formatLine(std::string_view text, LineCommentFormatOptions options = {}) {
    return formatLineComment(text, std::move(options));
  }
};

TEST(AbstractFormatterTest, FullFormattingSkipsRecoveredDocuments) {
  auto shared = test::make_shared_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_services(*shared, "mini", {".mini"})));

  workspace::Document document;
  document.uri = test::make_file_uri("recovered.mini");
  document.languageId = "mini";
  document.setText("entity A {}");
  document.parseResult.parseDiagnostics.push_back(parser::ParseDiagnostic{.offset = 0});

  CountingFormatter provider(lookup_services(*shared, "mini"));
  const auto edits = provider.formatDocument(
      document, ::lsp::DocumentFormattingParams{}, utils::default_cancel_token);

  EXPECT_TRUE(edits.empty());
  EXPECT_EQ(provider.callCount, 0u);
}

TEST(AbstractFormatterTest, RangeFormattingSkipsRangesWithEarlierParseErrors) {
  auto shared = test::make_shared_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_services(*shared, "mini", {".mini"})));

  workspace::Document document;
  document.uri = test::make_file_uri("range-error.mini");
  document.languageId = "mini";
  document.setText("entity A {}\nentity B {}");
  document.parseResult.parseDiagnostics.push_back(parser::ParseDiagnostic{.offset = 0});

  CountingFormatter provider(lookup_services(*shared, "mini"));
  ::lsp::DocumentRangeFormattingParams params{};
  params.range.start = text::Position(1, 0);
  params.range.end = text::Position(1, 10);

  const auto edits =
      provider.formatDocumentRange(document, params, utils::default_cancel_token);
  EXPECT_TRUE(edits.empty());
  EXPECT_EQ(provider.callCount, 0u);
}

TEST(AbstractFormatterTest, RegisteredCallbacksUseExactRuntimeTypeDispatch) {
  auto parsed = open_mini_document(
      "entity Alpha { first:T second:U }\n"
      "entity Beta {}");
  ASSERT_NE(parsed.document, nullptr);

  ExactDispatchFormatter provider(lookup_services(*parsed.shared, "mini"));
  const auto edits = provider.formatDocument(
      *parsed.document, ::lsp::DocumentFormattingParams{},
      utils::default_cancel_token);

  EXPECT_TRUE(edits.empty());
  EXPECT_EQ(provider.astNodeCount, 0u);
  EXPECT_EQ(provider.modelCount, 1u);
  EXPECT_EQ(provider.itemCount, 2u);
  EXPECT_EQ(provider.fieldCount, 2u);
}

TEST(AbstractFormatterTest, LaterFormatterRegistrationOverridesEarlierOne) {
  auto parsed = open_mini_document("entity   Alpha {}");
  ASSERT_NE(parsed.document, nullptr);

  OverridingRegistrationFormatter provider(lookup_services(*parsed.shared, "mini"));
  const auto edits = provider.formatDocument(
      *parsed.document, ::lsp::DocumentFormattingParams{},
      utils::default_cancel_token);

  ASSERT_EQ(edits.size(), 1u);
  EXPECT_EQ(apply_text_edits(*parsed.document, edits), "entity Alpha {}");
}

TEST(AbstractFormatterTest, LaterHiddenFormatterRegistrationOverridesEarlierOne) {
  auto parsed = open_mini_document(
      "entity Alpha {\n"
      "/**old\n"
      "@param   x   value\n"
      "*/\n"
      "}");
  ASSERT_NE(parsed.document, nullptr);

  ::lsp::DocumentFormattingParams params{};
  params.options.insertSpaces = true;
  params.options.tabSize = 2;

  OverridingHiddenRegistrationFormatter provider(
      lookup_services(*parsed.shared, "mini"));
  const auto edits =
      provider.formatDocument(*parsed.document, params,
                              utils::default_cancel_token);

  ASSERT_FALSE(edits.empty());
  EXPECT_EQ(apply_text_edits(*parsed.document, edits),
            "entity Alpha {\n"
            "  /**\n"
            "   * old\n"
            "   * @param x value\n"
            "   */\n"
            "}");
}

TEST(AbstractFormatterDslTest,
     TypedPropertyAndKeywordSelectionsTargetExpectedCstNodes) {
  auto parsed = open_mini_document("entity Alpha { many first:T second:U }");
  ASSERT_NE(parsed.document, nullptr);
  const auto *model = model_of(*parsed.document);
  ASSERT_NE(model, nullptr);
  ASSERT_EQ(model->items.size(), 1u);
  const auto &item = *model->items.front();

  std::vector<std::string> selected;
  FormattingBuilder builder([&selected](const CstNodeView &node, FormattingMode,
                                        const FormattingAction &) {
    selected.emplace_back(node.getText());
  });
  auto formatter = builder.getNodeFormatter(&item);
  const auto action = FormattingApiProbe::oneSpaceAction();

  formatter.property<&mini::ast::Item::name>().prepend(action);
  formatter.property<&mini::ast::Item::fields>(1).prepend(action);
  formatter.keyword("entity").prepend(action);
  formatter.keywords("{", "}").prepend(action);

  ASSERT_EQ(selected.size(), 5u);
  EXPECT_EQ(selected[0], "Alpha");
  EXPECT_EQ(selected[1], "second:U");
  EXPECT_EQ(selected[2], "entity");
  EXPECT_EQ(selected[3], "{");
  EXPECT_EQ(selected[4], "}");
}

TEST(AbstractFormatterDslTest,
     InteriorAndSliceSelectionsReturnExpectedNodes) {
  auto parsed = open_mini_document("entity Alpha { first:T second:U third:V }");
  ASSERT_NE(parsed.document, nullptr);
  const auto *model = model_of(*parsed.document);
  ASSERT_NE(model, nullptr);
  ASSERT_EQ(model->items.size(), 1u);
  const auto &item = *model->items.front();

  std::vector<std::string> selected;
  FormattingBuilder builder([&selected](const CstNodeView &node, FormattingMode,
                                        const FormattingAction &) {
    selected.emplace_back(node.getText());
  });

  auto formatter = builder.getNodeFormatter(&item);
  const auto openBrace = formatter.keyword("{");
  const auto closeBrace = formatter.keyword("}");
  formatter.interior(openBrace, closeBrace)
      .slice(1, 3)
      .prepend(FormattingApiProbe::oneSpaceAction());

  ASSERT_EQ(selected.size(), 2u);
  EXPECT_EQ(selected[0], "second:U");
  EXPECT_EQ(selected[1], "third:V");
}

TEST(AbstractFormatterDslTest, HiddenSelectionsReturnExpectedNodes) {
  auto parsed = open_mini_document(
      "entity Alpha { /*one*/ many first:T /*two*/ second:U }");
  ASSERT_NE(parsed.document, nullptr);
  const auto *model = model_of(*parsed.document);
  ASSERT_NE(model, nullptr);
  ASSERT_EQ(model->items.size(), 1u);
  const auto &item = *model->items.front();

  std::vector<std::string> selected;
  FormattingBuilder builder([&selected](const CstNodeView &node, FormattingMode,
                                        const FormattingAction &) {
    selected.emplace_back(node.getText());
  });

  auto formatter = builder.getNodeFormatter(&item);
  formatter.hidden("ML_COMMENT").prepend(FormattingApiProbe::oneSpaceAction());
  formatter.hiddens("ML_COMMENT").prepend(FormattingApiProbe::oneSpaceAction());

  ASSERT_EQ(selected.size(), 3u);
  EXPECT_EQ(selected[0], "/*one*/");
  EXPECT_EQ(selected[1], "/*one*/");
  EXPECT_EQ(selected[2], "/*two*/");
}

TEST(AbstractFormatterTest, OnTypeFormattingOnlyProducesEditsOnCurrentLine) {
  auto parsed = open_mini_document("entity Alpha {}\nentity    Beta {}");
  ASSERT_NE(parsed.document, nullptr);

  KeywordSpacingFormatter provider(lookup_services(*parsed.shared, "mini"));
  ::lsp::DocumentOnTypeFormattingParams params{};
  params.position.line = 1;
  params.position.character = 8;

  const auto edits =
      provider.formatDocumentOnType(*parsed.document, params,
                                    utils::default_cancel_token);
  ASSERT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits.front().range.start.line, 1u);
  EXPECT_EQ(edits.front().newText, " ");
}

TEST(AbstractFormatterTest, KeywordFormattingMatchesCaseInsensitiveLiterals) {
  auto parsed = open_mini_document("ENTITY    Alpha {}");
  ASSERT_NE(parsed.document, nullptr);

  KeywordSpacingFormatter provider(lookup_services(*parsed.shared, "mini"));
  ::lsp::DocumentFormattingParams params{};

  const auto edits =
      provider.formatDocument(*parsed.document, params,
                              utils::default_cancel_token);

  ASSERT_EQ(edits.size(), 1u);
  EXPECT_EQ(apply_text_edits(*parsed.document, edits), "ENTITY Alpha {}");
}

TEST(AbstractFormatterTest, HigherPriorityFormattingWinsForSameRegion) {
  ::lsp::DocumentFormattingParams params{};
  auto parsed = open_mini_document("entity   Alpha {}");
  ASSERT_NE(parsed.document, nullptr);

  PriorityFormatter provider(lookup_services(*parsed.shared, "mini"));
  const auto edits =
      provider.formatDocument(*parsed.document, params,
                              utils::default_cancel_token);

  ASSERT_EQ(edits.size(), 1u);
  EXPECT_EQ(apply_text_edits(*parsed.document, edits), "entity Alpha {}");
}

TEST(AbstractFormatterTest, OverlappingEditsKeepTheLaterSpecificFormatting) {
  ::lsp::DocumentFormattingParams params{};
  auto parsed = open_mini_document("entity   Alpha {}");
  ASSERT_NE(parsed.document, nullptr);

  OverlapFormatter provider(lookup_services(*parsed.shared, "mini"));
  const auto edits =
      provider.formatDocument(*parsed.document, params,
                              utils::default_cancel_token);

  ASSERT_EQ(edits.size(), 1u);
  EXPECT_EQ(apply_text_edits(*parsed.document, edits), "entity Alpha {}");
}

TEST(AbstractFormatterTest, AllowMoreAndAllowLessCanSuppressEdits) {
  ::lsp::DocumentFormattingParams params{};
  auto parsed = open_mini_document("entity    Alpha{}");
  ASSERT_NE(parsed.document, nullptr);

  FlexibleWhitespaceFormatter provider(lookup_services(*parsed.shared, "mini"));
  const auto edits =
      provider.formatDocument(*parsed.document, params,
                              utils::default_cancel_token);

  EXPECT_TRUE(edits.empty());
}

TEST(AbstractFormatterTest, FitChoosesSingleLineFormattingWhenApplicable) {
  ::lsp::DocumentFormattingParams params{};
  auto parsed = open_mini_document("entity Alpha   {}");
  ASSERT_NE(parsed.document, nullptr);

  FitFormatter provider(lookup_services(*parsed.shared, "mini"));
  const auto edits =
      provider.formatDocument(*parsed.document, params,
                              utils::default_cancel_token);

  ASSERT_EQ(edits.size(), 1u);
  EXPECT_EQ(apply_text_edits(*parsed.document, edits), "entity Alpha {}");
}

TEST(AbstractFormatterTest, PreservesCommentsAndOnlyReindentsHiddenNodes) {
  auto parsed = open_mini_document(
      "entity Alpha {\n"
      "// note\n"
      "many first:T\n"
      "second:U\n"
      "}");
  ASSERT_NE(parsed.document, nullptr);

  ::lsp::DocumentFormattingParams params{};
  params.options.insertSpaces = true;
  params.options.tabSize = 2;

  MiniLayoutFormatter provider(lookup_services(*parsed.shared, "mini"));
  const auto edits =
      provider.formatDocument(*parsed.document, params,
                              utils::default_cancel_token);

  ASSERT_FALSE(edits.empty());
  EXPECT_EQ(apply_text_edits(*parsed.document, edits),
            "entity Alpha {\n"
            "  // note\n"
            "  many first: T\n"
            "  second: U\n"
            "}");
}

TEST(AbstractFormatterTest, HiddenSelectionsCanFormatCommentsInScopedSubtrees) {
  auto parsed = open_mini_document(
      "entity Alpha {\n"
      "// note\n"
      "many first:T\n"
      "}");
  ASSERT_NE(parsed.document, nullptr);

  ::lsp::DocumentFormattingParams params{};
  params.options.insertSpaces = true;
  params.options.tabSize = 2;

  HiddenSelectionFormatter provider(lookup_services(*parsed.shared, "mini"));
  const auto edits =
      provider.formatDocument(*parsed.document, params,
                              utils::default_cancel_token);

  ASSERT_FALSE(edits.empty());
  EXPECT_EQ(apply_text_edits(*parsed.document, edits),
            "entity Alpha {\n"
            "  // note\n"
            "  many first: T\n"
            "}");
}

TEST(AbstractFormatterTest, HiddenNodeCallbacksCanReplaceCommentText) {
  auto parsed = open_mini_document(
      "entity Alpha {\n"
      "/*old\n"
      "line*/\n"
      "many first:T\n"
      "}");
  ASSERT_NE(parsed.document, nullptr);

  ::lsp::DocumentFormattingParams params{};
  params.options.insertSpaces = true;
  params.options.tabSize = 2;

  HiddenReplacementFormatter provider(lookup_services(*parsed.shared, "mini"));
  const auto edits =
      provider.formatDocument(*parsed.document, params,
                              utils::default_cancel_token);

  ASSERT_FALSE(edits.empty());
  EXPECT_EQ(apply_text_edits(*parsed.document, edits),
            "entity Alpha {\n"
            "  /* level 1\n"
            "   * updated\n"
            "   */\n"
            "  many first: T\n"
            "}");
}

TEST(AbstractFormatterTest, MultilineCommentUtilityNormalizesTagsAndContinuation) {
  EXPECT_EQ(FormattingApiProbe::formatComment(
                "/**\n"
                "*   Adds   numbers.\n"
                "* @param   x   first   value\n"
                "*   more   details\n"
                "*\n"
                "*/"),
            "/**\n"
            " * Adds numbers.\n"
            " * @param x first value\n"
            " *  more details\n"
            " */");
}

TEST(AbstractFormatterTest, MultilineCommentUtilityLeavesUnmatchedCommentsUntouched) {
  EXPECT_EQ(FormattingApiProbe::formatComment("/* keep me */"), "/* keep me */");
}

TEST(AbstractFormatterTest, LineCommentUtilityNormalizesSpacing) {
  EXPECT_EQ(FormattingApiProbe::formatLine("//   hello   world"),
            "// hello world");
}

TEST(AbstractFormatterTest, PreservesLeadingHiddenNodesBeforeRootContent) {
  auto parsed = open_mini_document(
      "    /** note */\n"
      "   entity Alpha {}");
  ASSERT_NE(parsed.document, nullptr);

  ::lsp::DocumentFormattingParams params{};
  params.options.insertSpaces = true;
  params.options.tabSize = 2;

  MiniLayoutFormatter provider(lookup_services(*parsed.shared, "mini"));
  const auto edits =
      provider.formatDocument(*parsed.document, params,
                              utils::default_cancel_token);

  ASSERT_FALSE(edits.empty());
  EXPECT_EQ(apply_text_edits(*parsed.document, edits),
            "/** note */\n"
            "entity Alpha {}");
}

} // namespace
} // namespace pegium::lsp
