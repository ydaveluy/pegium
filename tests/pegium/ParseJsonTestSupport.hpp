#pragma once

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>

#include <pegium/converter/AstJsonConverter.hpp>
#include <pegium/converter/CstJsonConverter.hpp>
#include <pegium/TestRuleParser.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::test {
namespace detail {

inline const parser::Skipper &default_skipper() {
  static const auto skipper = parser::SkipperBuilder().build();
  return skipper;
}

inline void parse_into_document(const parser::Parser &parser,
                                workspace::Document &document,
                                const parser::Skipper &,
                                const parser::ParseOptions &) {
  parser.parse(document);
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

} // namespace detail

template <typename Parseable>
std::unique_ptr<workspace::Document>
ParseDocument(const Parseable &parseable, std::string_view text,
              const parser::Skipper &skipper,
              const parser::ParseOptions &options = {}) {
  auto document = std::make_unique<workspace::Document>();
  document->setText(std::string{text});
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
ExpectCst(const workspace::Document &document, std::string_view expectedJson,
          const converter::CstJsonConversionOptions &options = {}) {
  EXPECT_EQ(CstJson(document, options), std::string(expectedJson));
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
