#pragma once

#include <atomic>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>

#include <pegium/converters/AstJsonConverter.hpp>
#include <pegium/converters/CstJsonConverter.hpp>
#include <pegium/TestRuleParser.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::test {
namespace detail {

inline const parser::Skipper &default_skipper() {
  static const auto skipper = parser::SkipperBuilder().build();
  return skipper;
}

inline workspace::DocumentId next_parse_test_document_id() noexcept {
  static std::atomic<workspace::DocumentId> nextDocumentId{1u};
  return nextDocumentId.fetch_add(1u, std::memory_order_relaxed);
}

inline std::shared_ptr<workspace::TextDocument>
make_text_document(std::string uri, std::string languageId, std::string_view text) {
  return std::make_shared<workspace::TextDocument>(workspace::TextDocument::create(
      std::move(uri), std::move(languageId), 0, std::string(text)));
}

inline void parse_into_document(const parser::Parser &parser,
                                workspace::Document &document,
                                const parser::Skipper &,
                                const parser::ParseOptions &) {
  apply_parse_result(document,
                     parser.parse(document.textDocument().getText()));
}

template <typename RuleType>
  requires std::derived_from<std::remove_cvref_t<RuleType>,
                             grammar::ParserRule>
inline void parse_into_document(const RuleType &parseable,
                                workspace::Document &document,
                                const parser::Skipper &skipper,
                                const parser::ParseOptions &options) {
  parse_rule(parseable, document, skipper, options);
}

inline parser::ParseResult parse(const parser::Parser &parser,
                                 std::string_view text,
                                 const parser::Skipper &,
                                 const parser::ParseOptions &) {
  return parser.parse(text);
}

template <typename RuleType>
  requires std::derived_from<std::remove_cvref_t<RuleType>,
                             grammar::ParserRule>
inline parser::ParseResult parse(const RuleType &parseable,
                                 std::string_view text,
                                 const parser::Skipper &skipper,
                                 const parser::ParseOptions &options) {
  return parse_rule_result(parseable, text, skipper, options);
}

} // namespace detail

template <typename Parseable>
parser::ParseResult Parse(const Parseable &parseable, std::string_view text,
                          const parser::Skipper &skipper,
                          const parser::ParseOptions &options = {}) {
  return detail::parse(parseable, text, skipper, options);
}

template <typename Parseable>
parser::ParseResult Parse(const Parseable &parseable, std::string_view text,
                          const parser::ParseOptions &options) {
  return Parse(parseable, text, detail::default_skipper(), options);
}

template <typename Parseable>
parser::ParseResult Parse(const Parseable &parseable, std::string_view text) {
  return Parse(parseable, text, detail::default_skipper(), {});
}

template <typename Parseable>
std::unique_ptr<workspace::Document>
ParseDocument(const Parseable &parseable, std::string_view text,
              const parser::Skipper &skipper,
              const parser::ParseOptions &options = {}) {
  const auto documentId = detail::next_parse_test_document_id();
  auto document = std::make_unique<workspace::Document>(
      detail::make_text_document("file:///parse-test-" +
                                     std::to_string(documentId),
                                 "test", std::string{text}));
  document->id = documentId;
  detail::parse_into_document(parseable, *document, skipper, options);
  return document;
}

template <typename Parseable>
std::unique_ptr<workspace::Document>
ParseDocument(const Parseable &parseable, std::string_view text,
              const parser::ParseOptions &options) {
  return ParseDocument(parseable, text, detail::default_skipper(), options);
}

template <typename Parseable>
std::unique_ptr<workspace::Document>
ParseDocument(const Parseable &parseable, std::string_view text) {
  return ParseDocument(parseable, text, detail::default_skipper(), {});
}

inline std::string
AstJson(const workspace::Document &document,
        const converter::AstJsonConversionOptions &options = {}) {
  EXPECT_NE(document.parseResult.value, nullptr);
  if (document.parseResult.value == nullptr) {
    return {};
  }
  return converter::AstJsonConverter::convert(*document.parseResult.value,
                                              options)
      .toJsonString();
}

inline std::string
AstJson(const parser::ParseResult &result,
        const converter::AstJsonConversionOptions &options = {}) {
  EXPECT_NE(result.value, nullptr);
  if (result.value == nullptr) {
    return {};
  }
  return converter::AstJsonConverter::convert(*result.value, options)
      .toJsonString();
}

inline std::string
CstJson(const workspace::Document &document,
        const converter::CstJsonConversionOptions &options = {}) {
  const auto root =
      document.parseResult.cst != nullptr
          ? std::addressof(*document.parseResult.cst)
          : (document.parseResult.value != nullptr &&
                     document.parseResult.value->hasCstNode()
                 ? std::addressof(document.parseResult.value->getCstNode().root())
                 : nullptr);
  EXPECT_NE(root, nullptr);
  if (root == nullptr) {
    return {};
  }
  return converter::CstJsonConverter::convert(*root, options).toJsonString();
}

inline std::string
CstJson(const parser::ParseResult &result,
        const converter::CstJsonConversionOptions &options = {}) {
  const auto root =
      result.cst != nullptr
          ? std::addressof(*result.cst)
          : (result.value != nullptr && result.value->hasCstNode()
                 ? std::addressof(result.value->getCstNode().root())
                 : nullptr);
  EXPECT_NE(root, nullptr);
  if (root == nullptr) {
    return {};
  }
  return converter::CstJsonConverter::convert(*root, options).toJsonString();
}

inline std::string
CstJson(const RootCstNode &root,
        const converter::CstJsonConversionOptions &options = {}) {
  return converter::CstJsonConverter::convert(root, options).toJsonString();
}

inline void
ExpectAst(const workspace::Document &document, std::string_view expectedJson,
          const converter::AstJsonConversionOptions &options = {}) {
  EXPECT_EQ(AstJson(document, options), std::string(expectedJson));
}

inline void
ExpectAst(const parser::ParseResult &result, std::string_view expectedJson,
          const converter::AstJsonConversionOptions &options = {}) {
  EXPECT_EQ(AstJson(result, options), std::string(expectedJson));
}

inline void
ExpectCst(const workspace::Document &document, std::string_view expectedJson,
          const converter::CstJsonConversionOptions &options = {}) {
  EXPECT_EQ(CstJson(document, options), std::string(expectedJson));
}

inline void
ExpectCst(const parser::ParseResult &result, std::string_view expectedJson,
          const converter::CstJsonConversionOptions &options = {}) {
  EXPECT_EQ(CstJson(result, options), std::string(expectedJson));
}

inline void
ExpectCst(const RootCstNode &root, std::string_view expectedJson,
          const converter::CstJsonConversionOptions &options = {}) {
  EXPECT_EQ(CstJson(root, options), std::string(expectedJson));
}

template <typename Parseable>
std::unique_ptr<workspace::Document>
ExpectAst(const Parseable &parseable, std::string_view text,
          std::string_view expectedJson,
          const parser::Skipper &skipper,
          const parser::ParseOptions &options = {},
          const converter::AstJsonConversionOptions &jsonOptions = {}) {
  auto document = ParseDocument(parseable, text, skipper, options);
  ExpectAst(*document, expectedJson, jsonOptions);
  return document;
}

template <typename Parseable>
parser::ParseResult
ExpectParsedAst(const Parseable &parseable, std::string_view text,
                std::string_view expectedJson, const parser::Skipper &skipper,
                const parser::ParseOptions &options = {},
                const converter::AstJsonConversionOptions &jsonOptions = {}) {
  auto result = Parse(parseable, text, skipper, options);
  ExpectAst(result, expectedJson, jsonOptions);
  return result;
}

template <typename Parseable>
parser::ParseResult
ExpectParsedAst(const Parseable &parseable, std::string_view text,
                std::string_view expectedJson,
                const parser::ParseOptions &options,
                const converter::AstJsonConversionOptions &jsonOptions = {}) {
  return ExpectParsedAst(parseable, text, expectedJson, detail::default_skipper(),
                         options, jsonOptions);
}

template <typename Parseable>
parser::ParseResult
ExpectParsedAst(const Parseable &parseable, std::string_view text,
                std::string_view expectedJson,
                const converter::AstJsonConversionOptions &jsonOptions = {}) {
  return ExpectParsedAst(parseable, text, expectedJson, detail::default_skipper(),
                         {}, jsonOptions);
}

template <typename Parseable>
std::unique_ptr<workspace::Document>
ExpectAst(const Parseable &parseable, std::string_view text,
          std::string_view expectedJson,
          const parser::ParseOptions &options,
          const converter::AstJsonConversionOptions &jsonOptions = {}) {
  return ExpectAst(parseable, text, expectedJson, detail::default_skipper(),
                   options, jsonOptions);
}

template <typename Parseable>
std::unique_ptr<workspace::Document>
ExpectAst(const Parseable &parseable, std::string_view text,
          std::string_view expectedJson,
          const converter::AstJsonConversionOptions &jsonOptions = {}) {
  return ExpectAst(parseable, text, expectedJson, detail::default_skipper(), {},
                   jsonOptions);
}

template <typename Parseable>
std::unique_ptr<workspace::Document>
ExpectCst(const Parseable &parseable, std::string_view text,
          std::string_view expectedJson,
          const parser::Skipper &skipper,
          const parser::ParseOptions &options = {},
          const converter::CstJsonConversionOptions &jsonOptions = {}) {
  auto document = ParseDocument(parseable, text, skipper, options);
  ExpectCst(*document, expectedJson, jsonOptions);
  return document;
}

template <typename Parseable>
std::unique_ptr<workspace::Document>
ExpectCst(const Parseable &parseable, std::string_view text,
          std::string_view expectedJson, const parser::ParseOptions &options,
          const converter::CstJsonConversionOptions &jsonOptions = {}) {
  return ExpectCst(parseable, text, expectedJson, detail::default_skipper(),
                   options, jsonOptions);
}

template <typename Parseable>
std::unique_ptr<workspace::Document>
ExpectCst(const Parseable &parseable, std::string_view text,
          std::string_view expectedJson,
          const converter::CstJsonConversionOptions &jsonOptions = {}) {
  return ExpectCst(parseable, text, expectedJson, detail::default_skipper(), {},
                   jsonOptions);
}

} // namespace pegium::test
